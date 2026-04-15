//***************************************************************************************
//Common included exception catcher for molecular updates
//fl = the file being updated
//uu = the current update unit
//***************************************************************************************

#define MOLECULE_CATCH(fl, uu)						\
catch (Exception& e) {								\
	uu->MoleculeExceptionHandler(&e, false, fl);	\
	throw "junk";									\
}													\
catch (std::bad_alloc) {							\
	uu->MoleculeExceptionHandler(NULL, true, fl);	\
	throw "junk";									\
}													\
catch (...) {										\
	uu->MoleculeExceptionHandler(NULL, false, fl);	\
	throw "junk";									\
}


