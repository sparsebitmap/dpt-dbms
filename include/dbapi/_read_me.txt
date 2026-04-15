DPT Database API Wrapper Classes
================================

- Refer to the API "how to" document for full details (DBAPI.html).

- To summarise, if you're calling the underlying DPT infrastructure directly you don't need the files in this directory or the corresponding "source" directory.

- The reason the wrappers are in a separate directory is that these header files have the same names as those containing the underlying wrapped classes.

- If using the wrappers, put this directory ahead of its parent in your compiler include paths list.  Alternatively just qualify all #include as e.g. "dbapi\dptdb.h".  (That header picks up everything).


