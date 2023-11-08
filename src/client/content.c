/* DERPY'S SCRIPT LOADER: CONTENT LOADER
	
	scripts can specify new files for the game to use in its IMG archives
	if any files are used for a certain archive then a new one is created in cache to be used
	
*/

#include <dsl/dsl.h>
#include <string.h>

#define PATH_BYTES 256
#define BLOCK_SIZE 2048
#define MAX_NAME_LENGTH 24
#define READ_MIN_BLOCKS 64
#define MIN_COPY_BUFFER 65536
#define HASHING_CHUNK_SIZE 8192

//#define HASH_ALL_CONTENT

// HASHES
static int g_hashes[CONTENT_TYPES];

// GENERAL UTILITY
static const char* getFileName(const char *path){
	const char *name;
	
	for(name = path;*path;path++)
		if(*path == '/' || *path == '\\')
			name = path + 1;
	return name;
}

// CONTENT HASHES
static int generateContentHash(const char *path){
	char buffer[HASHING_CHUNK_SIZE];
	DWORD bytes;
	HANDLE file;
	int hash;
	
	file = CreateFile(path,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(file == INVALID_HANDLE_VALUE)
		return 0;
	hash = 0;
	while(ReadFile(file,buffer,HASHING_CHUNK_SIZE,&bytes,NULL)){
		if(!bytes){
			CloseHandle(file);
			return hash;
		}
		while(bytes){
			hash *= 0x83;
			hash += buffer[--bytes];
		}
	}
	CloseHandle(file);
	return 0;
}
void generateContentHashes(){
	#ifdef HASH_ALL_CONTENT
	g_hashes[CONTENT_ACT_IMG] = generateContentHash("Act/Act.img");
	g_hashes[CONTENT_CUTS_IMG] = generateContentHash("Cuts/Cuts.img");
	g_hashes[CONTENT_TRIGGER_IMG] = generateContentHash("DAT/Trigger.img");
	g_hashes[CONTENT_IDE_IMG] = generateContentHash("Objects/ide.img");
	#endif
	g_hashes[CONTENT_SCRIPTS_IMG] = generateContentHash("Scripts/Scripts.img");
	#ifdef HASH_ALL_CONTENT
	g_hashes[CONTENT_WORLD_IMG] = generateContentHash("Stream/World.img");
	#endif
}
int getContentHash(int type){
	return g_hashes[type];
}

// MAIN TYPES
typedef struct content_file{
	loader_collection *lc;
	char path[PATH_BYTES];
	const char *name;
}content_file;
typedef struct script_content{
	content_file *files;
	unsigned index; // actual count
	unsigned count; // allocated count
	int replaced;
}script_content;

// CONTENT STATE
script_content* createContentManager(dsl_state *dsl){
	script_content *sc;
	
	sc = calloc(CONTENT_TYPES,sizeof(script_content));
	if(!sc)
		return NULL;
	return sc;
}
void destroyContentManager(script_content *sc){
	int type;
	
	for(type = 0;type < CONTENT_TYPES;type++)
		if(sc[type].files)
			free(sc[type].files);
	free(sc);
}
void freeContentListings(script_content *sc){
	int type;
	
	for(type = 0;type < CONTENT_TYPES;type++){
		free(sc->files);
		sc->files = NULL;
		sc->index = 1; // index && !files signals that this SC was free'd
		sc++;
	}
}
int wasContentReplaced(script_content *sc,int type){
	return sc[type].replaced;
}

// GET CONTENT
static content_file* getContent(script_content *sc,const char *name){
	content_file *files;
	unsigned count;
	
	files = sc->files;
	count = sc->index;
	while(count)
		if(!dslstrncmp(files[--count].name,name,MAX_NAME_LENGTH))
			return files + count;
	return NULL;
}

// ADD CONTENT
static void fixContentNames(content_file *f,unsigned count){
	for(;count;count--){
		f->name = getFileName(f->path);
		f++;
	}
}
int addContentFile(script_content *sc,loader_collection *lc,int type,const char *path){
	const char *name;
	content_file *f;
	unsigned count;
	void *block;
	
	sc += type;
	if(sc->index && !sc->files)
		return 0; // see freeContentListings
	if(strlen(path) >= PATH_BYTES || strlen(name = getFileName(path)) > MAX_NAME_LENGTH)
		return 0;
	if(f = getContent(sc,name)){
		f->lc = lc;
		f->name = getFileName(strcpy(f->path,path));
		return 1;
	}
	if(sc->index == sc->count){
		count = sc->count ? sc->count * 2 : 4;
		block = realloc(sc->files,sizeof(content_file)*count);
		if(!block)
			return 0;
		fixContentNames(block,sc->index);
		sc->count = count;
		sc->files = block;
	}
	f = sc->files + sc->index;
	f->lc = lc;
	f->name = getFileName(strcpy(f->path,path));
	sc->index++;
	return 1;
}
static void addContentFromCollection(dsl_state *dsl,loader_collection *lc,int type,const char *key){
	config_file *cfg;
	const char *path;
	int index;
	
	if(cfg = lc->cfg)
		for(index = 0;path = getConfigStringArray(cfg,key,index);index++)
			if(!addContentFile(dsl->content,lc,type,path))
				printConsoleError(dsl->console,"[%s] failed to register content (%s %s)",(char*)(lc+1),key,path);
}
void addContentUsingLoader(dsl_state *dsl){
	loader_collection *lc;
	
	for(lc = dsl->loader->first;lc;lc = lc->next){
		addContentFromCollection(dsl,lc,CONTENT_ACT_IMG,"act_file");
		addContentFromCollection(dsl,lc,CONTENT_CUTS_IMG,"cuts_file");
		addContentFromCollection(dsl,lc,CONTENT_TRIGGER_IMG,"trigger_file");
		addContentFromCollection(dsl,lc,CONTENT_IDE_IMG,"ide_file");
		addContentFromCollection(dsl,lc,CONTENT_SCRIPTS_IMG,"scripts_file");
		addContentFromCollection(dsl,lc,CONTENT_WORLD_IMG,"world_file");
	}
}

// BUILD TYPES
typedef struct dir_file{
	unsigned offset;
	unsigned size;
	char name[MAX_NAME_LENGTH];
}dir_file;
typedef struct build_state{
	HANDLE input;
	HANDLE output;
	unsigned count;
	dir_file *files;
	DWORD append_pos;
	script_content *sc;
}build_state;

// BUILD STATE
static void cleanupBuild(build_state *bs){
	if(bs->input != INVALID_HANDLE_VALUE)
		CloseHandle(bs->input);
	if(bs->output != INVALID_HANDLE_VALUE)
		CloseHandle(bs->output);
	if(bs->count)
		free(bs->files);
	free(bs);
}
static build_state* setupBuild(dsl_state *dsl,const char *spath,const char *dpath){
	build_state *bs;
	
	bs = malloc(sizeof(build_state));
	if(!bs){
		printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"content builder");
		return NULL;
	}
	bs->count = 0;
	bs->input = CreateFile(spath,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	bs->output = CreateFile(dpath,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if(bs->input == INVALID_HANDLE_VALUE || bs->output == INVALID_HANDLE_VALUE){
		printConsoleError(dsl->console,"failed to open file: %s",bs->input ? dpath : spath);
		cleanupBuild(bs);
		return NULL;
	}
	return bs;
}

// BUILD STAGES
static int loadDirFiles(build_state *bs,dsl_state *dsl,const char *name){
	DWORD size;
	DWORD read;
	
	size = GetFileSize(bs->input,NULL);
	if(!size || size == INVALID_FILE_SIZE || size % sizeof(dir_file)){
		printConsoleError(dsl->console,"invalid file size: %s",name);
		return 0;
	}
	bs->files = malloc(size + bs->sc->index * sizeof(dir_file)); // allocate enough space for the worse case scenario that everything is an addition and not a replacement
	if(!bs->files){
		printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"dir files");
		return 0;
	}
	bs->count = size / sizeof(dir_file);
	if(!ReadFile(bs->input,bs->files,size,&read,NULL) || read != size){
		printConsoleError(dsl->console,"failed to read file: %s",name);
		return 0;
	}
	return 1;
}
static void insertDirFile(dir_file *files,dir_file *file,unsigned count){
	while(count && file->offset > files->offset)
		count--,files++;
	while(count){
		count--;
		memcpy(files+count+1,files+count,sizeof(dir_file));
	}
	memcpy(files,file,sizeof(dir_file));
}
static int sortDirFiles(build_state *bs,dsl_state *dsl,const char *name){
	dir_file *source;
	dir_file *files;
	unsigned index;
	unsigned count;
	
	source = bs->files;
	files = malloc((bs->count + bs->sc->index) * sizeof(dir_file));
	if(!files){
		printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"dir sorter");
		return 0;
	}
	for(count = index = 0;index < bs->count;index++)
		if(!getContent(bs->sc,source[index].name))
			insertDirFile(files,source+index,count++);
	free(bs->files);
	bs->files = files;
	bs->count = count;
	return 1;
}
static unsigned preImgCopy(dir_file *files,unsigned count,DWORD base,DWORD limit,DWORD *result){
	unsigned dirs;
	DWORD resize;
	DWORD size;
	
	dirs = 0;
	size = 0;
	do{
		resize = size + files[dirs].size * BLOCK_SIZE;
		if(resize > limit)
			break;
		files[dirs].offset = (base + size) / BLOCK_SIZE;
		size = resize;
	}while(++dirs < count && !(files[dirs].offset * BLOCK_SIZE - (base + size)));
	*result = size;
	return dirs;
}
static int copyBaseImg(build_state *bs,dsl_state *dsl,const char *name){
	dir_file *file;
	unsigned index;
	unsigned dirs;
	char *buffer;
	DWORD bufsiz;
	DWORD bytes;
	DWORD size;
	DWORD pos; // file pointer
	
	CloseHandle(bs->input);
	bs->input = CreateFile(name,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(bs->input == INVALID_HANDLE_VALUE){
		printConsoleError(dsl->console,"failed to open file: %s",name);
		return 0;
	}
	file = bs->files;
	bufsiz = READ_MIN_BLOCKS;
	for(index = 0;index < bs->count;index++)
		if(file[index].size > bufsiz)
			bufsiz = file[index].size;
	bufsiz *= BLOCK_SIZE;
	buffer = malloc(bufsiz);
	if(!buffer){
		printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"img buffer");
		free(buffer);
		return 0;
	}
	pos = bs->append_pos = 0;
	for(index = 0;index < bs->count;index += dirs){
		file = bs->files + index;
		if(size = file->offset * BLOCK_SIZE - pos){ // do we need to skip over anything?
			if(SetFilePointer(bs->input,size,NULL,FILE_CURRENT) == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR){
				printConsoleError(dsl->console,"failed to seek file: %s (target: %.*s, win32: %d)",name,MAX_NAME_LENGTH,file->name,GetLastError());
				free(buffer);
				return 0;
			}
			pos += size;
		}
		dirs = preImgCopy(file,bs->count-index,bs->append_pos,bufsiz,&size); // get how many files to read at once and set offsets
		if(!ReadFile(bs->input,buffer,size,&bytes,NULL) || bytes != size || !WriteFile(bs->output,buffer,size,&bytes,NULL) || bytes != size){
			printConsoleError(dsl->console,"failed to copy file: %s (target: %.*s, buffer: %lu, bytes: %lu / %lu, win32: %d)",name,MAX_NAME_LENGTH,file->name,bufsiz,bytes,size,GetLastError());
			free(buffer);
			return 0;
		}
		bs->append_pos += size;
		pos += size;
	}
	free(buffer);
	return 1;
}
static int addContent(build_state *bs,dsl_state *dsl,content_file *cf,size_t *bufsiz,char **buffer){
	dir_file *file;
	dsl_file *f;
	DWORD bytes;
	long size;
	long zero;
	void *b;
	
	f = openDslFile(dsl,cf->lc,cf->path,"rb",&size);
	if(!f)
		return 0;
	if(zero = size % BLOCK_SIZE)
		zero = BLOCK_SIZE - zero; // bytes to zero
	if(*bufsiz < size + zero){
		b = malloc(size + zero);
		if(!b){
			printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"copy buffer");
			closeDslFile(f);
			return 0;
		}
		*bufsiz = size + zero;
		*buffer = b;
	}
	if(readDslFile(f,*buffer,size) != size){
		closeDslFile(f);
		return 0;
	}
	closeDslFile(f);
	if(zero){
		memset(*buffer+size,0,zero);
		size += zero;
	}
	if(!WriteFile(bs->output,*buffer,size,&bytes,NULL) || bytes != size){
		printConsoleError(dsl->console,"failed to append file: %s",cf->name);
		return 0;
	}
	file = bs->files + bs->count++;
	file->offset = bs->append_pos / BLOCK_SIZE;
	file->size = size / BLOCK_SIZE;
	strncpy(file->name,cf->name,MAX_NAME_LENGTH);
	bs->append_pos += size;
	return 1;
}
static int copyContent(build_state *bs,dsl_state *dsl){
	script_content *sc;
	unsigned count;
	size_t bufsiz;
	char *buffer;
	
	CloseHandle(bs->input);
	bs->input = INVALID_HANDLE_VALUE;
	sc = bs->sc;
	count = sc->index;
	buffer = malloc(bufsiz = MIN_COPY_BUFFER);
	if(!buffer){
		printConsoleError(dsl->console,TEXT_FAIL_ALLOCATE,"copy buffer");
		return 0;
	}
	while(count){
		count--;
		if(!addContent(bs,dsl,sc->files+count,&bufsiz,&buffer)){
			free(buffer);
			return 0;
		}
	}
	free(buffer);
	return 1;
}
static int writeOutDir(build_state *bs,dsl_state *dsl,const char *name){
	DWORD target;
	DWORD wrote;
	
	CloseHandle(bs->output);
	bs->output = CreateFile(name,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if(bs->output == INVALID_HANDLE_VALUE){
		printConsoleError(dsl->console,"failed to open file: %s",name);
		return 0;
	}
	target = bs->count * sizeof(dir_file);
	if(!WriteFile(bs->output,bs->files,target,&wrote,NULL) || wrote != target){
		printConsoleError(dsl->console,"failed to write file: %s",name);
		return 0;
	}
	return 1;
}

// BUILD ARCHIVE
int makeArchiveWithContent(dsl_state *dsl,int type,const char *source,const char *destination){
	script_content *sc;
	build_state *bs;
	char spath[32];
	char dpath[64];
	DWORD timer;
	
	sc = dsl->content + type;
	if(!sc->index)
		return 0;
	timer = GetTickCount();
	strcpy(spath,source);
	strcpy(dpath,destination);
	strcpy(spath+strlen(spath)-3,"dir");
	bs = setupBuild(dsl,spath,dpath); // 1. open input.dir and output.img
	if(!bs)
		return 0;
	bs->sc = sc;
	if(!loadDirFiles(bs,dsl,spath) || !sortDirFiles(bs,dsl,spath)){ // 2. load input.dir files and sort by offset while skipping files with replacements
		cleanupBuild(bs);
		return 0;
	}
	strcpy(spath+strlen(spath)-3,"img");
	if(!copyBaseImg(bs,dsl,spath) || !copyContent(bs,dsl)){ // 3. copy input.img to output.img and write all custom content
		cleanupBuild(bs);
		return 0;
	}
	strcpy(dpath+strlen(dpath)-3,"dir");
	if(!writeOutDir(bs,dsl,dpath)){ // 4. write output.dir
		cleanupBuild(bs);
		return 0;
	}
	sc->replaced = 1;
	cleanupBuild(bs);
	g_hashes[type] = generateContentHash(destination);
	printConsoleOutput(dsl->console,"built %.*s.img in %lu ms",strlen(dpath)-4,dpath,GetTickCount()-timer);
	return 1;
}