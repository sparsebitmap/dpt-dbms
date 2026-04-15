DPT Documentation Information
-----------------------------

Contents of the documentation
-----------------------------
The files in this directory constitute the complete set of user documentation provided with DPT. The doc copy that comes with a particular release of the executable files should match that release.  It is assumed that you are a Model 204 user, and therefore have access to the Model 204 documentation set as well.

Format
------
The html used in the documents is about as basic as it comes, and should display reasonably well on all browsers.

Note that the hyperlinks throughout the docs won't work if they're opened from within the zip file.


Installation notes re. Documentation
------------------------------------
Ideally this should not trouble you, but this info is here just in case it does.

By default when you invoke the "Help" menu item, the DPT host and client applications will start up your browser, and assume that the documentation .html files are in directory "<exe-directory>\..\Doc".  To clarify, the install process should leave you with something like this:

\DPT
	\Host
		dpthost.exe
		some sample batch jobs
		demo database files
		\DEMOPROC
			sample UL code
	\Client
		dptclient.exe
		sample batch2 job
	\Doc
		all the docs

which means the documentation is in its default place as far as both the client and host are concerned.

If you move the docs relatively, the client and host both need to be told where they are for F1 to work.  The "options" menu in both applications provides the means to do that.
