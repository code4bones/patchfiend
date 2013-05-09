// patchit.cpp : Defines the entry point for the console application.
//


#include <code4bones/confx.h>
#include <code4bones/cmdargs.h>
#include <code4bones/fileUtils.h>


class config : public confx::content {

	
};

config g_cfg;

void usage(char *name) {
        
    
    fprintf(stderr,"%s [OPTIONS] [KEYS]\n",name);
    fprintf(stderr," [OPTIONS]\n");
    fprintf(stderr,"  --db={file}                   - alternative patch db file\n");
    fprintf(stderr,"  --patch={name|patched_file})  - name from patch db to ovveride --orig\n");
    fprintf(stderr,"                                  or file path to compare with --orig,\n");
    fprintf(stderr,"                                  if --diff key is given\n");
    fprintf(stderr,"  --orig={file}                 - original file name to patch,or compare with\n");
    fprintf(stderr,"                                  if --diff key is given,and --path={patched_file}\n");
    fprintf(stderr,"  --asm=\"asm op;..asm op..\"     - generate assembler op codes (using platform build tools)\n");
    fprintf(stderr," [KEYS]\n");
    fprintf(stderr,"  --dirty                       - don't validate original bytes on while patch");
    fprintf(stderr,"  --diff                        - flag to compare --orig with --patch and produce\n");
    fprintf(stderr,"                                  patch db entry\n");
    fprintf(stderr,"  --view                        - parse and display patch db\n");
    fprintf(stderr,"  --check                       - don't patch,only try to apply the patch to --orig\n");
    fprintf(stderr,"  --bytes                       - --asm produces {0x..,0.xx} formated output to place it to db\n");
}

void init_conf(char **argv) {
	
	__vstring conf = get_exe_path();
	conf += "/patchfiend.conf";
	g_cfg["db"] = conf;

	cmdargs args;
	args << ARG(1,"patch",cmdargs::optional)
		 << ARG(2,"orig",cmdargs::optional)
		 << ARG(3,"db",cmdargs::optional)
		 << ARG(4,"view",cmdargs::key)
		 << ARG(5,"check",cmdargs::key)
		 << ARG(6,"file",cmdargs::optional)
		 << ARG(7,"diff",cmdargs::key)
		 << ARG(8,"asm",cmdargs::optional)
		 << ARG(9,"bytes",cmdargs::key)
                 << ARG(10,"dirty",cmdargs::key)
                 << ARG(11,"ascii",cmdargs::key);
	args.parse(argv);
	for ( cmdargs::cmd_arg *arg = args.first(); arg != 0; arg = args.next() ) {
		if ( !arg->isKey() && !arg->is_set())
			continue;
		switch ( arg->id ) {
		case 1: g_cfg["patch"] = arg->as_string();
			break;
		case 2: g_cfg["orig"] = arg->as_string();
			break;
		case 3: g_cfg["db"] = arg->as_string();
			break;
		case 4: g_cfg["view"] = "yes";
			break;
		case 5: g_cfg["check"] = "yes";
			break;
		case 6: g_cfg["file"] = arg->as_string();
			break;
		case 7: g_cfg["diff"] = "yes";
			break;
		case 8: g_cfg["asm"] = arg->as_string();
			break;
		case 9: g_cfg["bytes"] = "yes";
			break;
                case 10: g_cfg["dirty"] = "yes";
                        break;
                case 11: g_cfg["ascii"] = "yes";
                        break;
		default:
			fprintf(stderr,"Adding diff file\n");
			break;
		}
	}

	if ( g_cfg.hasOpt("asm") || g_cfg.hasOpt(("diff")) )
		return;
        if ( !g_cfg.hasOpt("patch") && !g_cfg.hasOpt("orig") && !g_cfg.hasOpt(("view")))
            return;
        
	fprintf(stderr," + reading db from %s\n",g_cfg["db"].c_str());
	confx cfg;
	std::fstream fs(g_cfg["db"].c_str(),std::fstream::in);
	cfg.setEOLTerm(false);
	cfg.parse(fs,&g_cfg);
}


void diff() {

	if ( !g_cfg.hasOpt("orig") )
		throw_runtime("original file name not given ( --orig={file} )");
	if ( !g_cfg.hasOpt("patch") )
		throw_runtime("patched file name not given ( --patch={file} )");

	fileUtils origFile(g_cfg["orig"]);
	fileUtils patchedFile(g_cfg["patch"]);
	
	fprintf(stderr," + Generating patch from\n + ORIGINAL : %s\n + PATCHED :  %s\n",origFile.source(),patchedFile.source());
	unsigned char *pOrig = (unsigned char*)origFile.map();
	unsigned char *pPatched = (unsigned char*)patchedFile.map();

	unsigned long origSize = origFile.mapSize();
	unsigned long patchedSize = patchedFile.mapSize();

	if ( origSize != patchedSize )
		throw_runtime("file size mismatch %s != %s",origFile.name(),patchedFile.name());
	
	enum { PREV,CUR,START,LAST_ };
	unsigned long patch[LAST_] = {-1};
	typedef std::list<unsigned char> vbyte;
	typedef std::map<unsigned long,vbyte> mofs;
	mofs origOfs,patchedOfs;

	__vstring _ofs = "";
	for ( unsigned long ofs = 0; ofs < origSize ;ofs++ ) {
		if ( pOrig[ofs] == pPatched[ofs] ) 
			continue;
		
		patch[PREV] = patch[CUR];
		patch[CUR] = ofs;
		if ( patch[PREV] + 1 != patch[CUR] || ofs == 0 ) {
			char buf[16];
			sprintf(buf,"0x%08lX",ofs);
			if ( !_ofs.empty() )
				_ofs += ",";
			_ofs += buf;
			patch[START] = patch[CUR];
		} 
		vbyte &origBytes = origOfs[patch[START]];
		origBytes.push_back(pOrig[ofs]);
		vbyte &patchBytes = patchedOfs[patch[START]];
		patchBytes.push_back(pPatched[ofs]);
	}
	
	if ( origOfs.empty() ) {
		fprintf(stderr,"files are identical...\n");
		return;
	}
		
	
	fprintf(stdout,"\n\n");
	fprintf(stdout,"########################\n");
	fprintf(stdout,"# Patch generated from\n");
	fprintf(stdout,"# original %s\n",origFile.source());
	fprintf(stdout,"# patched  %s\n",patchedFile.source());
	fprintf(stdout,"# apply with patchit [--patch=\"%s\"] --orig=<path to>/%s\n\n",origFile.name(),patchedFile.name());
	fprintf(stdout,"[%s]\n",origFile.name());
	fprintf(stdout,"# Patching offset list\n");
	fprintf(stdout,"ofs { %s };\n\n",_ofs.c_str());
	fprintf(stdout,"# check/write pairs for each offset\n");

	mofs::iterator iPatched = patchedOfs.begin();
	mofs::iterator iOrig    = origOfs.begin();

	for (; iOrig != origOfs.end(); iOrig++,iPatched++ ) {

		vbyte &origBytes  = iOrig->second;
		vbyte &patchBytes = iPatched->second;
		
		vbyte::iterator iOrigByte  = origBytes.begin();
		vbyte::iterator iPatchByte = patchBytes.begin();

		__vstring check,write;
		for (; iOrigByte != origBytes.end();iOrigByte++,iPatchByte++ ) {
			char buf[16];
			if ( !check.empty() )
				check += ",";
			if ( !write.empty() )
				write += ",";
			sprintf(buf,"0x%02X",*iOrigByte);
			check += buf;
			sprintf(buf,"0x%02X",*iPatchByte);
			write += buf;
		}
		fprintf(stdout,"original at 0x%08lX { %s }; # %ld bytes\n",iOrig->first,check.c_str(),origBytes.size());
		fprintf(stdout,"patching at 0x%08lX { %s };\n",iPatched->first,write.c_str());
	}
	fprintf(stdout,"#### END OF %s PATCH #####\n",origFile.name(true));
}

#ifndef WIN32
#define _popen popen
#define _pclose pclose
#endif

void code() {
	struct del {
		~del() { 
			unlink("./code.c");
			unlink("./code.obj");
		}
	} _del;
	__vstring s = g_cfg["asm"];

	FILE *fd = fopen("./code.c","w+t");
	if ( !fd ) 
		throw_runtime(" - %s",err_msg);	

	fprintf(stdout,"# %s\n",s.c_str());
#ifdef WIN32
	s = replace(s,";","\n");
	fprintf(fd,"__declspec(naked) void code() { __asm { %s }; } ",s.c_str());
#else
	fprintf(fd,"void code() { __asm__ (\"%s\"); } ",s.c_str());
#endif
        fclose(fd);

	char cmd[1024];

#ifdef WIN32
	sprintf(cmd,"cl /c /nologo code.c");
#else
        sprintf(cmd,"gcc -c -march=i386 -m32 -mtune=i386 ./code.c -o code.obj");
#endif
        fd = _popen(cmd,"r");
	while ( !feof(fd) ) {
		char *ptr = fgets(cmd,sizeof(cmd),fd);
		if ( ptr == 0 )
			break;
		//fprintf(stderr,"%s",ptr);
	}
	int ec = _pclose(fd);
	if ( ec != 0 && ec != 2 ) 
		throw_runtime("compiler error %d",ec);
#ifdef WIN32
	sprintf(cmd,"dumpbin /DISASM code.obj");
#else
        sprintf(cmd,"objdump -M intel -D ./code.obj");
#endif
        fd = _popen(cmd,"r");
	if ( fd == 0 )
		throw_runtime("exec: %s -> %s",cmd,err_msg);
	
	bool fbytes = g_cfg["bytes"].boolVal(false);

	confx::content::enum_type ops;
	__vstring str;
	while ( !feof(fd) ) {
		char *ptr = fgets(cmd,sizeof(cmd),fd);
		if ( ptr == 0 )
			break;
#ifdef WIN32
		if ( strstr(ptr,"0000") == 0 )
			continue;
		if ( !fbytes )
#endif
			fprintf(stderr,"%s",ptr);
#ifdef WIN32
		else {
			__vstring line(&ptr[12]);
			__vstring::size_type pos = line.find("  ");
			if ( pos != __vstring::npos ) {
				line = line.substr(0,pos).c_str();
				char *p = (char*)line.c_str();
				char *op = strtok(p," ");
				while ( op ) {
					char buf[16];
					sprintf(buf,"0x%s",op);
					if ( !str.empty() )
						str+=",";
					str += buf;
					op = strtok(0," ");
				}
			}
		} // bytes - true
#endif
	}
#ifdef WIN32
	if ( fbytes )
	fprintf(stdout,"patching at <offset> { %s };\n",str.c_str());
#endif
        ec = _pclose(fd);
	return;
}

void patch() {


	__vstring origFile;
	__vstring patchName; 

	if ( !g_cfg.hasOpt("orig") )
		throw_runtime("original file to patch not given ( --orig={file} )");
	else
		origFile = g_cfg["orig"];

	if ( g_cfg.hasOpt("patch") ) {
		patchName = g_cfg["patch"];
		fprintf(stderr," * using overriden patch name from --patch arg : %s\n",patchName.c_str());
	}
	else {
		fprintf(stderr," * extracting patch name from original file name: %s\n",origFile.c_str());
		fileUtils fu(g_cfg["orig"] );
		patchName = fu.name();
	}

	fprintf(stderr," + looking up patch entry for %s\n",patchName.c_str());
	
	if ( !g_cfg.hasSection(patchName) ) 
		throw_runtime(" - cannot find patch entry with name %s ( case-sensetive )",patchName.c_str());
	bool test= g_cfg.hasOpt("check");
	
	if ( test ) 
		fprintf(stderr,"** CHECK MODE **\n");

        
        bool dirty = g_cfg.hasOpt("dirty");
        bool ascii = g_cfg.hasOpt("ascii");
        
        if ( dirty )
            fprintf(stderr,"** DIRTY MODE, no original bytes will be checked\n");
        if ( ascii )
            fprintf(stderr,"** ASCII MODE patching...\n");
        
	__vstring ofsName = patchName  + ".ofs";
	if ( !g_cfg.hasEnum(ofsName) ) 
		throw_runtime("cannot find offset list ! (ofs { ... };)\n"); 
	confx::content::enum_type& offList = g_cfg.getEnum(ofsName);
	fprintf(stderr," + found %ld offsets to patch at..\n",offList.size());
	for ( confx::content::enum_type::iterator iofs = offList.begin(); iofs != offList.end(); iofs++ ) {
		unsigned long ofs = iofs->longVal();
		fprintf(stderr,"...seeking to 0x%08lX ( %ld )\n",ofs,ofs);
		char buf[64];

		sprintf(buf,"%s.patching at 0x%08lX",patchName.c_str(),ofs);
		if ( !g_cfg.hasEnum(buf) ) 
			throw_runtime("missing original bytes entry ( original at 0x%X { .. }; )",ofs);
		confx::content::enum_type& wrList = g_cfg.getEnum(buf);

		fileUtils fu(origFile.c_str());
		unsigned char *pFile = (unsigned char*)fu.map();
                
                // PATCH BYTES
                int nBytes = wrList.size();
                unsigned char *wrBytes = (unsigned char*)malloc(nBytes+1);
                fprintf(stderr," + found %ld bytes to apply the patch\n",wrList.size());
                int pos = 0;
		for ( confx::content::enum_type::iterator ibyte = wrList.begin(); ibyte != wrList.end(); ibyte++,pos++ ) {  
                    if ( !ascii ) 
                        wrBytes[pos] = (unsigned char)ibyte->longVal();	
                    else {    
                        if ( ibyte->length() == 1 )
                            wrBytes[pos] = *ibyte;//->c_str()[0];
                        else
                            wrBytes[pos] = (unsigned char)ibyte->longVal();
                    }
                }
                
                if ( !dirty ) {
                    // ORIGINAL BYTES AND CHECK
                    sprintf(buf,"%s.original at 0x%08lX",patchName.c_str(),ofs);
                    if ( !g_cfg.hasEnum(buf) ) 
                            throw_runtime("missing original bytes entry ( original at 0x%X { .. }; )",ofs);
                    confx::content::enum_type& rdList = g_cfg.getEnum(buf);

                    if ( rdList.size() != wrList.size() && !dirty )
                            throw_runtime("number of original bytes doesn't match to number of patch bytes");

                    unsigned char *pBytes = (unsigned char*)malloc(nBytes+1);
                    int pos = 0;

                    for ( confx::content::enum_type::iterator ibyte = rdList.begin(); ibyte != rdList.end(); ibyte++,pos++ ) 
                    if ( !ascii ) 
                        pBytes[pos] = (unsigned char)ibyte->longVal();	
                    else {    
                        if ( ibyte->length() == 1 )
                            pBytes[pos] = *ibyte;//->c_str()[0];
                        else
                            pBytes[pos] = (unsigned char)ibyte->longVal();
                    }


         	    int res;
                    if ( (res = memcmp(&pFile[ofs],pBytes,nBytes)) != 0 ) {
                            fprintf(stderr," - %d bytes at offset 0x%08lX ( %ld ) are differs from original\n",nBytes,ofs,ofs);
                            res = memcmp(&pFile[ofs],wrBytes,nBytes);			
                            if ( res == 0 )
                                    fprintf(stderr," - file already patched\n");
                            else 
                                    fprintf(stderr," - file content changed,but not with the patch bytes...skipping\n");
                            free(pBytes);
                            free(wrBytes);
                            return;
                    } 
                    free(pBytes);
                } // dirty == false

                if ( !test ) {
                    memcpy(&pFile[ofs],wrBytes,nBytes);
                    fprintf(stderr," + %d bytes writed at offset 0x%08lX ( %ld )\n",nBytes,ofs,ofs);
		} else {
                    fprintf(stderr," * CHECK MODE * patch not applied\n");
                }
                if ( ascii ) {
                    wrBytes[nBytes] = 0;
                    fprintf(stderr," + ASCII [%s]\n",wrBytes);
                
                }
                free(wrBytes);
	}
	fprintf(stderr,"*** %s Patched - ok\n",patchName.c_str());
}


int main(int argc, char* argv[])
{
	try {
		init_conf(argv);
		if ( g_cfg.hasOpt("view") ) {
			g_cfg.dump();
			return 0;
		}

		if ( g_cfg.hasOpt("diff") )
			diff();
		else if ( g_cfg.hasOpt("patch") || g_cfg.hasOpt("orig") )
			patch();
		else if ( g_cfg.hasOpt("asm") )
			code();
		else
			usage(argv[0]);
	} catch ( std::runtime_error &e ) {
		fprintf(stderr," ** ERROR **\n%s\n",e.what());
	}
	return 0;
}

