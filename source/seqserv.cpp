
#include "stdafx.h"

#include "seqserv.h"
#include "windows.h"

#include <vector>

//Utils
#include "winutil.h"
#include "bbstdio.h"
#include "dataconv.h"
#include "parsing.h"
//API Tiers
#include "file.h"
#include "seqfile.h"
#include "dbserv.h"
#include "core.h"
#include "msgroute.h"
//Diagnostics
#include "except.h"
#include "msg_seq.h"
#include "msg_file.h"

namespace dpt {

ThreadSafeLong SequentialFileServices::instances = ThreadSafeLong();
std::string SequentialFileServices::seqtemp = "#SEQTEMP";
std::string SequentialFileServices::seqtemp_path = "#SEQTEMP";

//*****************************************************************************************
SequentialFileServices::SequentialFileServices(DatabaseServices* d)
: dbapi(d) 
{
	instances.Inc();

	//V2.06 May 2007.  Cribbed this from procedure services - see detailed comments there.
	if (instances.Value() == 1) {

		CoreServices* core = dbapi->Core();

		int rc = win::RemoveDirectoryTree(seqtemp.c_str());
		if (rc == ERROR_SUCCESS) {
			core->GetRouter()->Issue(SEQ_TEMPDIR_ERROR, 
				"Warning: Deleted #SEQTEMP directory remaining from last run");
		}
		else if (rc != ERROR_FILE_NOT_FOUND) {
			core->GetRouter()->Issue(SEQ_TEMPDIR_ERROR, 
				"Warning: Could not delete still-existing #SEQTEMP directory, "
				"creating dummy version");
			core->GetRouter()->Issue(SEQ_TEMPDIR_ERROR, 
				"* * * Possible system bug: please report if this happens a lot * * *");

			seqtemp = win::GetUnusedFileName("#SEQTEMP");
		}

		if (!CreateDirectory(seqtemp.c_str(), NULL)) {
			throw Exception(SEQ_TEMPDIR_ERROR, std::string 
				("System directory ")
				.append(seqtemp)
				.append(" creation failed, OS message was: ")
				.append(win::GetLastErrorMessage()));
		}

		//V2.15.  Cache fullpath now so calling code can work more freely with CWD etc.
		char fpbuff[_MAX_PATH];
		if (_fullpath(fpbuff, seqtemp.c_str(), _MAX_PATH))
			seqtemp_path = fpbuff;
	}
}

//*****************************************************************************************
SequentialFileServices::~SequentialFileServices()
{
	MsgRouter* router = dbapi->Core()->GetRouter();

	//Close any views the user still has open
	std::set<SequentialFileView*>::iterator i = open_directory.begin();
	while (i != open_directory.end()) {
		CloseSeqFile(*i);
		i = open_directory.begin();
	}
		
	//Free all still-allocated files if this is the last user
	instances.Dec();
	if (instances.Value() == 0) {

		std::vector<FileHandle> info;
		AllocatedFile::ListAllocatedFiles(info, BOOL_EXCL, FILETYPE_SEQ);

		for (size_t x = 0; x < info.size(); x++) {
			std::string dd = info[x].GetDD();
			std::string dsn = info[x].GetDSN();
		
			//Much the same code as in Free.  Try/catch to ensure each file gets a turn.
			//Since this is the last user, the frees should in theory not fail.
			try {
				std::string msg("Sequential file freed at system closedown: ");
				msg.append(dd);

				info[x].StageForFree();
				bool tempdsn = SequentialFile::Destroy(info[x]);
				router->Issue(SYSFILE_FREED_FINAL, msg);

				//V2.06 May 2007.
				if (tempdsn) {
					util::StdioDeleteFile(dsn.c_str());
					router->Issue(SEQ_FILE_DELETED, 
						std::string("Temporary sequential file deleted: ").append(dsn));
				}
			}
			catch (...) {
				std::string msg("Bug! FREE failed at system closedown: ");
				msg.append(dd);
				dbapi->Core()->GetRouter()->Issue(BUG_MISC, msg);
			}
		}

		//V2.22 Oct 09.  Now since $FILE_OPEN can create temp files, that aren't subject to 'FREE'
		//processing (plus new API function on SequentialFileServices to the same effect) now we
		//should clear down detritus if possible.
		int ndel = DeleteOldTempFiles(".*");
		if (ndel) {
			std::string msg = util::IntToString(ndel);
			msg.append(" ad-hoc temporary files deleted from #SEQTEMP");
			router->Issue(SEQ_FILE_DELETED, msg);
		}

		//V2.06.  May 2007.  The reverse of the constructor - see above.
		if (!RemoveDirectory(seqtemp.c_str())) {
			router->Issue(SEQ_TEMPDIR_ERROR, std::string
				("System directory ")
				.append(seqtemp)
				.append(" was not deleted, OS message was: ")
				.append(win::GetLastErrorMessage()));
		}
	}
}


//*****************************************************************************************
void SequentialFileServices::Allocate
(const std::string& dd, const std::string& dsn, FileDisp disp, 
 int lrecl, char pad, unsigned int max, const std::string& alias, bool nocrlf)
{
	bool wascreated = false;
	std::string realdsn = dsn;

	//V2.06.  May 2007.  TEMP means generate a dummy DSN here, then it's as NEW.
	bool tempdsn = false;
	if (disp == FILEDISP_TEMP) {
		if (dsn.length() != 0)
			throw Exception(SEQ_BAD_DSN, "DSN must not be given for TEMP files");

		realdsn = MakeNewTempFileName();		
		disp = FILEDISP_NEW;
		tempdsn = true;
	}
	
	FileHandle h = SequentialFile::Construct
		(dd, realdsn, lrecl, pad, max, disp, wascreated, alias, tempdsn, nocrlf);
	h.CommitAllocation();

	if (wascreated) {
		std::string msg = (tempdsn) 
			? "Temporary sequential file created: " 
			: "Sequential file created: "; 
		dbapi->Core()->GetRouter()->Issue(SEQ_FILE_CREATED, std::string(msg).append(realdsn));
	}

	std::string msg("Sequential file allocated: ");
	msg.append(dd).append(1, '=').append(realdsn);
	if (alias.length() != 0)
		msg.append(" (AltName=").append(alias).append(1, ')');
	dbapi->Core()->GetRouter()->Issue(SYSFILE_ALLOCATED, msg);
}

//*****************************************************************************************
void SequentialFileServices::Free(const std::string& dd)
{
	FileHandle fh = GetHandle(dd, BOOL_EXCL);

	std::string alias = fh.GetAlias();
	std::string realdd = fh.GetDD();
	std::string realdsn = fh.GetDSN();

	fh.StageForFree();
	bool tempdsn = SequentialFile::Destroy(fh);
	
	std::string msg = std::string("Sequential file freed: ").append(realdd);
	if (alias.length() != 0)
		msg.append(" (AltName=").append(alias).append(1, ')');

	dbapi->Core()->GetRouter()->Issue(SYSFILE_FREED, msg);

	//V2.06 May 2007.
	if (tempdsn) {
		util::StdioDeleteFile(realdsn.c_str());
		dbapi->Core()->GetRouter()->Issue(SEQ_FILE_DELETED, 
			std::string("Temporary sequential file deleted: ").append(realdsn));
	}
}

//*****************************************************************************************
FileHandle SequentialFileServices::GetHandle(const std::string& dd, bool lk)
{
	FileHandle fh = AllocatedFile::FindAllocatedFile(dd, lk, FILETYPE_SEQ);

	//See comments in procserv and the FREE command
	if (fh.GetFile() == NULL)
		throw Exception(SYSFILE_NOT_ALLOCATED, std::string
			("Sequential file is not allocated: ").append(dd));

	if (fh.GetType() != FILETYPE_SEQ)
		throw Exception(SYSFILE_BAD_TYPE, std::string
			("File is allocated, but not sequential: ").append(dd));

	return fh;
}

//*****************************************************************************************
SequentialFileView* SequentialFileServices::OpenSeqFile(const std::string& dd, bool lk)
{
	//Fairly simple here so long as the file has been allocated
	FileHandle fh = GetHandle(dd, BOOL_SHR);
	SequentialFile* sf = static_cast<SequentialFile*>(fh.GetFile());

	SequentialFileView* sv = new SequentialFileView(this, sf, lk);
	sv->hfile = fh;

	try {
		open_directory.insert(sv);
	}
	catch (...) {
		delete sv;
		throw;
	}

	return sv;
}

//*****************************************************************************************
void SequentialFileServices::CloseSeqFile(SequentialFileView* sv)
{
	delete sv;
	open_directory.erase(sv);
}

//*****************************************************************************************
std::string SequentialFileServices::MakeNewTempFileName(const std::string& extn)
{
	std::string result = seqtemp;
	result.append("\\F");
	result.append(util::IntToString(time(NULL)));
	result.append(extn);

	//Just in case the time isn't unique enough
	result = win::GetUnusedFileName(result);

	return result;
}

//*****************************************************************************************
//V2.22 Sep 09.  The SEQTEMP directory is handy because we can just leave files in there
//and be sure they'll get deleted in the end.  (But see next function).
std::string SequentialFileServices::CreateNewTempFile
(const std::string& extn, int* leaveopen_handle)
{
	std::string fname = MakeNewTempFileName(extn);

	int h = util::StdioSopen(fname.c_str(), util::STDIO_NEW);

	if (leaveopen_handle)
		*leaveopen_handle = h;
	else
		util::StdioClose(h);

	return fname;
}

//*****************************************************************************************
int SequentialFileServices::DeleteOldTempFiles(const std::string& extn, int agesecs)
{
	int now = time(NULL);
	int delbefore = now - agesecs;

	std::string filepatt = seqtemp;
	filepatt.append("\\F*");
	filepatt.append(extn);

	//Loop on the directory
	int files_deleted = 0;

	WIN32_FIND_DATA fileinfo;
	HANDLE handle = FindFirstFile(filepatt.c_str(), &fileinfo);

	int more = (handle == INVALID_HANDLE_VALUE) ? 0 : 1;
	while (more) {
		//Filenames will be e.g. F1234567.TXT or F1234567_2.TXT
		std::string dosfile = fileinfo.cFileName;
		int timeend = dosfile.find_first_of("._");
		std::string stime = dosfile.substr(1, timeend-1);

		if (!util::IsAlpha(stime)) {
			int create_time = util::StringToInt(stime);

			if (create_time <= delbefore) {
				std::string dospath = std::string(seqtemp).append("\\").append(dosfile);

				//In case file still being used somewhere
				try {
					util::StdioDeleteFile(dospath.c_str());
					files_deleted++;
				}
				catch (...) {}
			}
		}

		more = FindNextFile(handle, &fileinfo);
	}
	FindClose(handle);

	return files_deleted;
}


} //close namespace

