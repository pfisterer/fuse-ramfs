#include <map>
#include <string>
#include <iostream>
using namespace std;

extern "C" {

#define FUSE_USE_VERSION  26
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

typedef map<string, map<int, unsigned char> > FileMap;
static FileMap files;

/** Prüft, ob eine Datei breits existiert */
static bool file_exists(string filename) {
	bool b = files.find(filename) != files.end();
	cout << "file_exists: " << filename << ": " << b << endl;
	return b;
}

/** Konvertiert einen String in eine map mit einzelnen bytes */
map<int, unsigned char> to_map(string data) {
	map<int, unsigned char> data_map;
	int i = 0;
	
	for (string::iterator it = data.begin() ; it < data.end(); ++it)
		data_map[i++] = *it;
		
	return data_map;
}

/** Entfernt einen potenziell vorhandenen / am Anfang */
static string strip_leading_slash(string filename) {
	bool starts_with_slash = false;
	
	if( filename.size() > 0 )
		if( filename[0] == '/' )
			starts_with_slash = true;
		
	return starts_with_slash ? filename.substr(1, string::npos) : filename;
}

/** Liefert Dateiattribute zurück */
static int ramfs_getattr(const char* path, struct stat* stbuf) {
	string filename = strip_leading_slash(filename);
	memset(stbuf, 0, sizeof(struct stat));
	
	//Setze owner and group auf die des aktuell eingeloggten Users
	//stbuf->st_uid = getuid();
	//stbuf->st_gid = getgid();
	
	if(string(path) == "/") { //Attribute des Wurzelverzeichnisses
		cout << "ramfs_getattr("<<filename<<"): Returning attributes for /" << endl;
		stbuf->st_mode = S_IFDIR | 0777;
		stbuf->st_nlink = 2;
		
	} else if(file_exists(filename)) { //Existierende Datei wird gelesen
		cout << "ramfs_getattr("<<filename<<"): Returning attributes" << endl;
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = files[filename].size();
		
	} else { //Datei nicht vorhanden
		cout << "ramfs_getattr("<<filename<<"): not found" << endl;
		return -ENOENT;
	}

	return 0;
}

/** Liest den Inhalt eines Verzeichnisses aus */
static int ramfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
							off_t offset, struct fuse_file_info* fi) {
								
	//Dieses Dateisystem kennt keine Unterverzeichnisse
	if(strcmp(path, "/") != 0) {
		cout << "ramfs_readdir("<<path<<"): Only / allowed" << endl;
		return -ENOENT;
	}

	//Diese Dateien müssen immer existieren
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
	//Füge alle Dateien hinzu
	for (FileMap::iterator it = files.begin(); it != files.end(); it++) 
		filler(buf, it->first.c_str(), NULL, 0);
	
	return 0;
}

/** "Öffnet" eine Datei */
static int ramfs_open(const char *path, struct fuse_file_info *fi) {
	string filename = strip_leading_slash(path);
	
	//Datei nicht vorhanden
	if( !file_exists(filename) ) {
		cout << "ramfs_readdir("<<filename<<"): Not found" << endl;
		return -ENOENT;
	}

	return 0;
}

/* Liest (Teile einer) Datei */
static int ramfs_read(const char* path, char* buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
	string filename = strip_leading_slash(path);

	//Datei nicht vorhanden
	if( !file_exists(filename) ) {
		cout << "ramfs_read("<<filename<<"): Not found" << endl;
		return -ENOENT;
	}
	
	//Datei existiert. Lese in Puffer
	map<int, unsigned char>& file = files[filename];
	size_t len = file.size();
	
	//Prüfe, wieviele Bytes ab welchem Offset gelesen werden können
	if (offset < len) {
		if (offset + size > len) {
			cout << "ramfs_read("<<filename<<"): offset("<<offset<<
				") + size("<<size<<") > len("<<len<<"), setting to " << len - offset << endl;
			
			size = len - offset;
		}

		cout << "ramfs_read("<<filename<<"): Reading "<< size << " bytes"<<endl;
		for(size_t i = 0; i < size; ++i)
			buf[i] = file[offset + i];
	} else { //Offset war groesser als die max. Groesse der Datei
		return -EINVAL;
	}

	return size;
}

/** Erzeugt ein neues Dateisystemelement */
int ramfs_mknod(const char *path, mode_t mode, dev_t dev) {
	string filename = strip_leading_slash(path);
	
	//Pruefe auf Fehler
	if( file_exists(filename) ) { //Datei bereits vorhanden
		cout << "ramfs_mknod("<<filename<<"): Already exists" << endl;
		return -EEXIST;
	} else if( (mode & S_IFREG) == 0) { //Nur regulaere Daten unterstuetzt
		cout << "ramfs_mknod("<<filename<<"): Only files may be created" << endl;
		return -EINVAL;
	}

	//Erzeuge Datei
	cout << "ramfs_mknod("<<filename<<"): Creating empty file" << endl;
	files[filename] = to_map("");
	return 0;
}

/** Schreibt Daten in eine (offene) Datei */
int ramfs_write(const char* path, const char* data, size_t size, off_t offset, struct fuse_file_info*) {
	string filename = strip_leading_slash(path);
	
	//Datei nicht vorhanden
	if( !file_exists(filename) ) {
		cout << "ramfs_write("<<filename<<"): Not found" << endl;
		return -ENOENT;
	}

	//Datei existiert. Schreibe Daten in Puffer
	cout << "ramfs_write("<<filename<<"): Writing "<< size << " bytes startting with offset "<< offset<<endl;
	map<int, unsigned char> &file = files[filename];
	
	for(size_t i = 0; i < size; ++i)
		file[offset + i] = data[i];
		
	return size;
}

/** Löscht eine Datei */
int ramfs_unlink(const char *pathname) {
	files.erase( strip_leading_slash(pathname) );
	return 0;
}

//Enthält die Funktionspointer auf die implementierten Operationen
static struct fuse_operations ramfs_oper;

int main(int argc, char *argv[])
{
	//Zuweisen der einzelnen Funktionspointer
	ramfs_oper.getattr	= ramfs_getattr;
	ramfs_oper.readdir 	= ramfs_readdir;
	ramfs_oper.open   	= ramfs_open;
	ramfs_oper.read   	= ramfs_read;
	ramfs_oper.mknod	= ramfs_mknod;
	ramfs_oper.write 	= ramfs_write;
	ramfs_oper.unlink 	= ramfs_unlink;
	
	//Starten des Dateisystems
	cout << "ramfs starting" << endl;
	return fuse_main(argc, argv, &ramfs_oper, NULL);
}

}