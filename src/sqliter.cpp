//Copyright 2015 Nathan Bossett

#include "sqliter.h"


csqliter::csqliter(void)
{
	db = NULL;
	litestmnt = NULL;
}


csqliter::~csqliter(void)
{
}

dbresulttype csqliter::openexistingdb(std::string &fullpath) {
//DB must exist for success (won't create a new one)
	return(openorcreate(fullpath, false));
}

dbresulttype csqliter::openexistingdb(const char *fullpath) {
//DB must already exist for success (won't create a new one)
	std::string full;
	log("opendb - char * piping to str");
	full = fullpath;
	return(openexistingdb(full));
}

dbresulttype csqliter::createdb(std::string &fullpath) {
	//sqlite doesn't let us _only_ create, only (open must exist | create) or (open must exist)
	//so, try opening first, if that fails try creating.  If opening succeeds, it's a failure
	//from our point of view
	dbresulttype rslt;
	rslt = openorcreate(fullpath, false);
	if (!rslt) {
		//that should not have worked
		closedb();
		return(otherdberror);
	}
	return(openorcreate(fullpath, true));
}

dbresulttype csqliter::createdb(const char *fullpath) {
std::string str;
	str = fullpath;
	return(createdb(str));
}

dbresulttype csqliter::openorcreate(std::string &fullpath, bool create) {
	//note: per "Using SQLite", p 463, you can't just pass in SQLITE_OPEN_CREATE but have to choose
	//	between opening with the possibility of creation or opening without the possibility of creating
	int flags;
	int rc;
	log("openorcreate:");
	log(fullpath.c_str());
	log("\n");

	if (create) {
		flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	} else {
		flags = SQLITE_OPEN_READWRITE;
	}
	
	rc = sqlite3_open_v2(fullpath.c_str(), &db, flags, NULL);
	switch(rc) {
	case SQLITE_OK:
		//...excellent
		break;
	default:
		db = NULL;
		log("\tsqlite3_open failed", rc);
		return(otherdberror);
	}

	rc = sqlite3_busy_timeout(db, DB_DEFAULT_TIMEOUT_MILLISECONDS);
	switch (rc) {
	case SQLITE_OK:
		return(successdb);
		break;
	default:
		log("\tsqlite3_busy_timeout failed", rc);
		return(otherdberror);
	}

}

dbresulttype csqliter::removedatabase(const char *fullpath) { 
//std::remove is only defined in terms of success or failure, not detailed error codes
	if (std::remove(fullpath)) {
		return(otherdberror);
	} else {
		return(successdb);
	}
}

dbresulttype csqliter::removedatabase(std::string &fullpath) {
	return(removedatabase(fullpath.c_str()));
}


sqlite3 *csqliter::getdbhandle() {
	return(db);
}

void csqliter::setdbhandle(sqlite3 *dbhandle) {
	finalizestatement();
	db = dbhandle;
}

void csqliter::bindstr(std::string &name, std::string &arg) {
	statementdirty = true;
	valsin.resize(valsin.size()+1);
	valsin[valsin.size()-1].name = name;
	valsin[valsin.size()-1].val.valtype = strdbval;
	valsin[valsin.size()-1].val.sval = arg;
}

void csqliter::bindstr(std::string &name, const char *arg) {
std::string ar;
	ar = arg;
	bindstr(name,ar);
}

void csqliter::bindstr(const char *name, std::string &arg) {
std::string nam;
	nam = name;
	bindstr(nam, arg);
}

void csqliter::bindstr(const char *name, const char *arg) {
std::string nam;
std::string ar;

	nam = name;
	ar = arg;

	bindstr(nam,ar);

}

void csqliter::bindint(std::string &name, sqlite3_int64 arg) {//int arg) {
	statementdirty = true;

	valsin.resize(valsin.size()+1);
	valsin[valsin.size()-1].name = name;
	valsin[valsin.size()-1].val.valtype = intdbval;
	valsin[valsin.size()-1].val.ival = arg;
}

void csqliter::bindint(const char *name, sqlite3_int64 arg) {//int arg) {
std::string nam;

	nam = name;
	bindint(nam,arg);

}

void csqliter::bindfloat(std::string &name, double arg) {
	statementdirty = true;

	valsin.resize(valsin.size()+1);
	valsin[valsin.size()-1].name = name;
	valsin[valsin.size()-1].val.valtype = floatdbval;
	valsin[valsin.size()-1].val.fval = arg;
	
}

void csqliter::bindfloat(const char *name, double arg) {
std::string nam;

	nam = name;
	bindfloat(nam, arg);

}

void csqliter::bindnull(std::string &name) {
	statementdirty = true;

	valsin.resize(valsin.size() + 1);
	valsin[valsin.size()-1].name = name;
	valsin[valsin.size()-1].val.valtype = nulldbval;
}

void csqliter::bindnull(const char *name) {
std::string nam;
	
	nam = name;
	bindnull(nam);

}

void csqliter::bindblob(std::string &name, unsignedcharvector &arg) {
	statementdirty = true;

	valsin.resize(valsin.size() + 1);
	valsin[valsin.size()-1].name = name;
	valsin[valsin.size()-1].val.valtype = blobdbval;
	valsin[valsin.size()-1].val.blob = arg;

}

void csqliter::bindblob(const char *name, unsignedcharvector &arg) {
std::string nam;

	nam = name;
	bindblob(nam,arg);

}

int csqliter::bindparameter(cdbparameter &param) {
std::string name;
int index;
int rc;

	if (db == NULL) {
		return(1);
	}

	name = ":" + param.name;
	index = sqlite3_bind_parameter_index(litestmnt, name.c_str());
	if (index == 0) {
		return(binddberror);
	}

	switch (param.val.valtype) {
	case intdbval:
		rc = sqlite3_bind_int64(litestmnt, index, param.val.ival);
		if (rc != SQLITE_OK) {
			return(binddberror);
		}
		break;
	case floatdbval:
		rc = sqlite3_bind_double(litestmnt, index, param.val.fval);
		if (rc != SQLITE_OK) {
			return(binddberror);
		}
		break;
	case strdbval:
		rc = sqlite3_bind_text(litestmnt, index, param.val.sval.c_str(), param.val.sval.length(), SQLITE_TRANSIENT);
		if (rc != SQLITE_OK) {
			return(binddberror);
		}
		break;
	case blobdbval:
		rc = sqlite3_bind_blob(litestmnt, index, &(param.val.blob[0]), param.val.blob.size(), SQLITE_TRANSIENT);
		if (rc != SQLITE_OK) {
			return(binddberror);
		}
		break;
	case nulldbval:
		rc = sqlite3_bind_null(litestmnt, index);
		break;
	default:
		return(binddberror);
	}

	return(successdb);


}

void csqliter::pushvaltypeout(dbvaltype valtype) {
	valtypesout.push_back(valtype);
}

void csqliter::pushvaltypesout(dbvaltype valtype) {
	pushvaltypeout(valtype);
}
void csqliter::pushvaltypesout(dbvaltype typ1, dbvaltype typ2) {
	pushvaltypeout(typ1);
	pushvaltypeout(typ2);
}
void csqliter::pushvaltypesout(dbvaltype typ1, dbvaltype typ2, dbvaltype typ3) {
	pushvaltypeout(typ1);
	pushvaltypeout(typ2);
	pushvaltypeout(typ3);
}
void csqliter::pushvaltypesout(dbvaltype typ1, dbvaltype typ2, dbvaltype typ3, dbvaltype typ4) {
	pushvaltypeout(typ1);
	pushvaltypeout(typ2);
	pushvaltypeout(typ3);
	pushvaltypeout(typ4);
}
void csqliter::pushvaltypesout(dbvaltype typ1, dbvaltype typ2, dbvaltype typ3, dbvaltype typ4, dbvaltype typ5) {
	pushvaltypeout(typ1);
	pushvaltypeout(typ2);
	pushvaltypeout(typ3);
	pushvaltypeout(typ4);
	pushvaltypeout(typ5);
}
void csqliter::pushvaltypesout(dbvaltype typ1, dbvaltype typ2, dbvaltype typ3, dbvaltype typ4, dbvaltype typ5, dbvaltype typ6) {
	pushvaltypeout(typ1);
	pushvaltypeout(typ2);
	pushvaltypeout(typ3);
	pushvaltypeout(typ4);
	pushvaltypeout(typ5);
	pushvaltypeout(typ6);
}
void csqliter::pushvaltypesout(dbvaltype typ1, dbvaltype typ2, dbvaltype typ3, dbvaltype typ4, dbvaltype typ5, dbvaltype typ6, dbvaltype typ7) {
	pushvaltypeout(typ1);
	pushvaltypeout(typ2);
	pushvaltypeout(typ3);
	pushvaltypeout(typ4);
	pushvaltypeout(typ5);
	pushvaltypeout(typ6);
	pushvaltypeout(typ7);
}
void csqliter::pushvaltypesout(dbvaltype typ1, dbvaltype typ2, dbvaltype typ3, dbvaltype typ4, dbvaltype typ5, dbvaltype typ6, dbvaltype typ7, dbvaltype typ8) {
	pushvaltypeout(typ1);
	pushvaltypeout(typ2);
	pushvaltypeout(typ3);
	pushvaltypeout(typ4);
	pushvaltypeout(typ5);
	pushvaltypeout(typ6);
	pushvaltypeout(typ7);
	pushvaltypeout(typ8);
}
void csqliter::pushvaltypesout(dbvaltype typ1, dbvaltype typ2, dbvaltype typ3, dbvaltype typ4, dbvaltype typ5, dbvaltype typ6, dbvaltype typ7, dbvaltype typ8, dbvaltype typ9) {
	pushvaltypeout(typ1);
	pushvaltypeout(typ2);
	pushvaltypeout(typ3);
	pushvaltypeout(typ4);
	pushvaltypeout(typ5);
	pushvaltypeout(typ6);
	pushvaltypeout(typ7);
	pushvaltypeout(typ8);
	pushvaltypeout(typ9);
}
void csqliter::pushvaltypesout(dbvaltype typ1, dbvaltype typ2, dbvaltype typ3, dbvaltype typ4, dbvaltype typ5, dbvaltype typ6, dbvaltype typ7, dbvaltype typ8, dbvaltype typ9, dbvaltype typ10) {
	pushvaltypeout(typ1);
	pushvaltypeout(typ2);
	pushvaltypeout(typ3);
	pushvaltypeout(typ4);
	pushvaltypeout(typ5);
	pushvaltypeout(typ6);
	pushvaltypeout(typ7);
	pushvaltypeout(typ8);
	pushvaltypeout(typ9);
	pushvaltypeout(typ10);
}

void csqliter::setsql(std::string &sql) {
	setsql(sql.c_str());
}

void csqliter::setsql(const char *sql) {
	finalizestatement();
	statementdirty = true;
	stmntsql = sql;
}

dbresulttype csqliter::compilesql() {
int rc;

	log("compilesql\n");

	if (db == NULL) {
		log("\tdb is null\n");
		return(otherdberror);
	}

	if (stmntsql == "") {
		log("\tsql statement is null\n");
		return(otherdberror);
	}

	if (litestmnt != NULL) {
		litestmnt = NULL;
		sqlite3_finalize(litestmnt);
	}
	rc = sqlite3_prepare_v2(db, stmntsql.c_str(), -1, &litestmnt, NULL);

	switch (rc) {
	case SQLITE_OK:
		return(successdb);
	case SQLITE_BUSY:
		log("\tdb is busy");
		return(busydberror);
	default:
		log("\tfailed",rc);
		return(otherdberror);
	}
}

dbresulttype csqliter::compilestatement() {
dbresulttype rval;
int rslt;
unsigned int i;

	log("compilestatement");
	log("\t");log(stmntsql);log("\n");

	valcountandtypesmatch = true;
	lastinsertrowid = 0;

	rval = compilesql();
	if (rval != successdb) {
		finalizestatement();
		log("\tcompilesql failed",rval);
		return(rval);
	}

	for (i=0;i < valsin.size(); i++) {
		rslt = bindparameter(valsin[i]);
		if (rslt) {
			finalizestatement();
			log("\tbind error\n");
			return(binddberror);
		}
	}

	return(successdb);

}

dbresulttype csqliter::runstep() {
dbresulttype rval;

	if (statementdirty) {
		//rval = compilesql();
		//if (rval) {
		//	return(rval);
		//}
		rval = compilestatement();
		if (rval) {
			return(rval);
		}
		statementdirty = false;
	}

	rval = stepstatement();

	switch (rval) {
	case successdb:
		finalizestatement();
		return(successdb);
		break;
	case rowresultdb:
		//extract the results
		return(rowresultdb);
		break;
	default:
		return(rval);
	}
	
	return(successdb);
}

dbresulttype csqliter::stepstatement() {
int rc;
int numout;
int coltype;
int size;
int i;
int pos;

	if (db == NULL) {
		return(otherdberror);
	}

	//done = false;

	rc = sqlite3_step(litestmnt);

	switch (rc) {
	case SQLITE_BUSY:
		return(busydberror);
	case SQLITE_DONE:
		lastinsertrowid = sqlite3_last_insert_rowid(db);
		return(successdb);
	case SQLITE_ROW:
		numout = sqlite3_column_count(litestmnt);
		if (numout != valtypesout.size()) {
			if (options.valcountmismatchcausesfailure) {
				valcountandtypesmatch = false;
				return(resultcountmismatchdberror);
			}
		}

		if (rowdata.size() != numout) {rowdata.resize(numout);}
		for (i=0;i<numout;i++) {
			coltype = sqlite3_column_type(litestmnt, i);
			if (options.valtypemismatchcausesfailure) {
				//if the types not matching is an error
				if (coltype != valtypesout[i]) {
					//...and if the types don't match
					if (coltype == nulldbval) {
						//the expected and actual types didn't match.  The actual is a null; therefore the expected isn't.
						if (options.nullisavaltypemismatch) {
							//...and if, should the type be returned is null, then is it a permitted or a not
							//	permitted null to find in a field like an INT or TEXT?
							valcountandtypesmatch = false;
							return(resulttypemismatchdberror);}
						else {
							//it's a NULL when we were expecting INT, TEXT, etc.
							//however, the options have not chosen to flag that as an error
							//(as is quite normal to SQL devs but maybe not all newer SQLiter users)

							//OK, fall back out of the conditionals and process as regular data.
						}
					} else {
						return(resulttypemismatchdberror);
					}
				}
			}
			switch (coltype) {
			case intdbval:
				rowdata[i].valtype = intdbval;
				rowdata[i].ival = (int) sqlite3_column_int(litestmnt,i);
				break;
			case floatdbval:
				rowdata[i].valtype = floatdbval;
				rowdata[i].fval = (double) sqlite3_column_double(litestmnt, i);
				break;
			case strdbval:
				rowdata[i].valtype = strdbval;
				rowdata[i].sval = (char *) sqlite3_column_text(litestmnt,i);
				break;
			case blobdbval:
				rowdata[i].valtype = blobdbval;
				size = sqlite3_column_bytes(litestmnt,i);
				rowdata[i].blob.resize(size);
				if (size > 0) {
					const unsigned char *blb;
					std::vector<unsigned char>::iterator it;
					std::string foo;
					blb = (const unsigned char*) sqlite3_column_blob(litestmnt,i);
					size = sqlite3_column_bytes(litestmnt, i);
					rowdata[i].blob.assign(blb, blb + size);
				}
				break;
			case nulldbval:
				rowdata[i].valtype = nulldbval;
				rowdata[i].ival = 0;
				rowdata[i].sval = "";
				break;
			default:
				return(otherdberror);
			}
		}
		return(rowresultdb);
	default:
		return(otherdberror);
	}

}

dbresulttype csqliter::runsinglestepstatement() {
	int rval;
	return(runsinglestepstatement(rval));
}

dbresulttype csqliter::runsinglestepstatement(int &numresultrowsreturned) {
//reduces boilerplate code by providing a single call which can return zero or one result rows.
//examples: a query by ID.
dbvals firstrow;
dbresulttype rval;
int count;

	count = 0;
	
	switch((rval=runstep())) {
	case successdb:
		//it worked and returned no results
		return(successdb);
		break;
	case rowresultdb:
		numresultrowsreturned = 0;
		//make sure there's only the one and that the final result is success.
		firstrow = rowdata;
		switch ((rval = runstep())) {
		case successdb:
			rowdata = firstrow;
			numresultrowsreturned = 1;
			return(successdb);
		default:
			return(rval);
		}
		break;
	default:
		return(rval);
	}

}

int csqliter::finalizestatement() {
	valsin.resize(0);
	valtypesout.resize(0);
	log("finalizestatement\n");
	if (db == NULL) {
		log("\tdb is null");
		return(1);
	}

	if (litestmnt == NULL) {
		log("\tstatement is already null");
		return(0);
	}
	sqlite3_finalize(litestmnt);
	litestmnt = NULL;
	return(0);

}

dbresulttype csqliter::closedb() {
int rc;
	if (litestmnt != NULL) {
		sqlite3_finalize(litestmnt);
	}
	rc = sqlite3_close(db);
	db = NULL;
	switch (rc) {
	case 0:
		log("closedb");log("\tok");
		return(successdb);
	default:
		log("closedb");log("\tfail");
		return(otherdberror);
	}
}

inline void csqliter::log(std::string &str) {
	log(str.c_str());
}

inline void csqliter::log(const char *str) {
	if (logfile.good()) {
		logfile << str;
	}
}

inline void csqliter::log(std::string &str, int sqlite_errorcode) {
	log(str.c_str(), sqlite_errorcode);
}

inline void csqliter::log(const char *str, int sqlite_errorcode) {
	std::string errstr;
	std::string logstr;

	geterrorname(sqlite_errorcode, errstr);
	logstr = str;
	logstr += "(SQLITE INTERNAL CODE:";
	logstr += errstr;
	logstr += ")";
	log(logstr);
}

int csqliter::startlog(const char *fullpath) {
	std::string fp;
	fp = fullpath;
	return(startlog(fp));
}

int csqliter::startlog(std::string &fullpath) {
	log("opening log file - WHILE ALREADY OPEN\n\t");
	log(fullpath.c_str());
	log("\n");
	logfile.open(fullpath.c_str());

	if (!(logfile.good())) {
		return(1);
	}

	log("startlog\n\t");
	log(fullpath.c_str());
	log("\n");

	return(0);

}

void csqliter::stoplog() {
	log("stoplog.\n");
	logfile.close();
}

csqliteroptions::csqliteroptions(void) {
	valtypemismatchcausesfailure = true;
	nullisavaltypemismatch = false;
	valcountmismatchcausesfailure = true;
}

void csqliteroptions::setvaltypemismatchcausesfailure(bool mismatchisfailure) {
	valtypemismatchcausesfailure = mismatchisfailure;
}

void csqliteroptions::setvalcountmismatchcausesfailure(bool mismatchisfailure) {
	valcountmismatchcausesfailure = mismatchisfailure;
}

void geterrorname(int code, std::string &str) {
	switch (code) {
//Error codes native to the SQLite layer, modified to report string errors
	case SQLITE_ERROR: str=      "SQL error or missing database";break;
	case SQLITE_INTERNAL: str=    "Internal logic error in SQLite";break;
	case SQLITE_PERM: str=        "Access permission denied";break;
	case SQLITE_ABORT: str=       "Callback routine requested an abort";break;
	case SQLITE_BUSY: str=        "The database file is locked";break;
	case SQLITE_LOCKED: str=      "A table in the database is locked";break;
	case SQLITE_NOMEM: str=       "A malloc() failed";break;
	case SQLITE_READONLY: str=    "Attempt to write a readonly database";break;
	case SQLITE_INTERRUPT: str=   "Operation terminated by sqlite3_interrupt()";break;
	case SQLITE_IOERR: str=       "Some kind of disk I/O error occurred";break;
	case SQLITE_CORRUPT: str=     "The database disk image is malformed";break;
	case SQLITE_NOTFOUND: str=    "Unknown opcode in sqlite3_file_control()";break;
	case SQLITE_FULL: str=        "Insertion failed because database is full";break;
	case SQLITE_CANTOPEN: str=    "Unable to open the database file";break;
	case SQLITE_PROTOCOL: str=    "Database lock protocol error";break;
	case SQLITE_EMPTY: str=      "Database is empty";break;
	case SQLITE_SCHEMA: str=      "The database schema changed";break;
	case SQLITE_TOOBIG: str=      "String or BLOB exceeds size limit";break;
	case SQLITE_CONSTRAINT: str=  "Abort due to constraint violation";break;
	case SQLITE_MISMATCH: str=    "Data type mismatch";break;
	case SQLITE_MISUSE: str=      "Library used incorrectly";break;
	case SQLITE_NOLFS: str=       "Uses OS features not supported on host";break;
	case SQLITE_AUTH: str=        "Authorization denied";break;
	case SQLITE_FORMAT: str=      "Auxiliary database format error";break;
	case SQLITE_RANGE: str=       "2nd parameter to sqlite3_bind out of range";break;
	case SQLITE_NOTADB: str=      "File opened that is not a database file";break;
	case SQLITE_NOTICE: str=      "Notifications from sqlite3_log()";break;
	case SQLITE_WARNING: str=     "Warnings from sqlite3_log()";break;
	case SQLITE_ROW: str=         "sqlite3_step() has another row ready";break;
	case SQLITE_DONE: str=        "sqlite3_step() has finished executing";break;
	default:
		str = "UNKOWN SQLITE ERROR CODE";
		break;
	}
}

int csqliter::runinternaltests() {
//this function tests some things which are most easily done with access to the private members.  An alternative
//	would be declaring a friend class but that starts additing conditionals.
//an example is assuring that equivalent functions actually are, such as using either a char* or a &string
std::string name;
std::string val;
std::string str;
char nameaschar[1024];
int i;

	valsin.clear();
	name = "testname";
	val = "testval";
	bindstr(name, val);
	bindstr(name, "testval");
	bindstr("testname", val);
	bindstr("testname", "testval");
	for (i=0;i<=3;i++) {
		if (valsin[i].val.valtype != strdbval) {
			return(1);
		}
		if (valsin[i].name != "testname") {
			return(1);
		}
		if (valsin[i].val.sval != "testval") {
			return(1);
		}
	}

	valsin.clear();
	bindint(name, 3);
	bindint("testname",3);
	for (i=0;i<=1;i++) {
		if (valsin[i].val.valtype != intdbval) {
			return(2);
		}
		if (valsin[i].name != "testname") {
			return(2);
		}
		if (valsin[i].val.ival != 3) {
			return(2);
		}
	}

	valsin.clear();
	bindfloat(name,3.14);
	bindfloat("testname",3.14);
	for (i=0;i<=1;i++) {
		if (valsin[i].val.valtype != floatdbval) {
			return(3);
		}
		if (valsin[i].name != "testname") {
			return(3);
		}
		if (valsin[i].val.fval != 3.14) {
			return(3);
		}
	}

	unsignedcharvector blob;
	valsin.clear();
	blob.resize(1);
	blob[0] = 4;
	bindblob(name,blob);
	bindblob("testname", blob);
	for (i=0;i<=1;i++) {
		if (valsin[i].val.valtype != blobdbval) {
			return(4);
		}
		if (valsin[i].name != "testname") {
			return(4);
		}
		if (valsin[i].val.blob.size() != 1) {
			return(4);
		}
		if (valsin[i].val.blob[0] != 4) {
			return(4);
		}
	}

	//picking float because it's not a possible variable default value like 0 or 1
	pushvaltypeout(floatdbval);
	pushvaltypesout(floatdbval);
	pushvaltypesout(floatdbval, floatdbval);
	pushvaltypesout(floatdbval, floatdbval, floatdbval);
	pushvaltypesout(floatdbval, floatdbval, floatdbval, floatdbval);
	pushvaltypesout(floatdbval, floatdbval, floatdbval, floatdbval, floatdbval);
	pushvaltypesout(floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval);
	pushvaltypesout(floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval);
	pushvaltypesout(floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval);
	pushvaltypesout(floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval);
	pushvaltypesout(floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval, floatdbval);
	if (valtypesout.size() != 56) {
		return(5);
	}
	for (i=0;i<=55;i++) {
		if (valtypesout[i] != floatdbval) {
			return(5);
		}
	}

	//just testing the polymorphism
	//testing here depends on the implementation... here we know that one of the functions does the work,
	//	so if the other causes any of the work to be done the piping through must have worked (in this case, setting the internal stmntsql variable
	//and elsewhere we have plenty of testing to make sure that the SQL is actually processed correctly
	setsql("Hello World!");
	if (stmntsql != "Hello World!") {
		return(6);
	}
	
	str = "Hello World!";
	stmntsql = "...";
	setsql(str);
	if (stmntsql != "Hello World!") {
		return(6);
	}

	finalizestatement();

	return(0);

}

