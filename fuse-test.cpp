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

typedef map<string, map<int, unsigned char> > FileMap;
static FileMap files;

/** Prüft, ob eine Datei breits existiert */
static bool file_exists(string filename) 
{
	bool b = files.find(filename) != files.end();
	cout << "file_exists: " << filename << ": " << b << endl;
	return b;
}

/** Konvertiert einen String in eine map mit einzelnen bytes */
map<int, unsigned char> to_map(string data) 
{
	map<int, unsigned char> data_map;
	int i = 0;
	
	for (string::iterator it = data.begin() ; it < data.end(); ++it)
		data_map[i++] = *it;
		
	return data_map;
}

/** Entfernt einen potenziell vorhandenen / am Anfang */
static string strip_leading_slash(string filename) 
{
	bool starts_with_slash = false;
	
	if( filename.size() > 0 )
		if( filename[0] == '/' )
			starts_with_slash = true;
		
	return starts_with_slash ? filename.substr(1, string::npos) : filename;
}

/** Liefert Dateiattribute zurück */
static int ramfs_getattr(const char *path, struct stat *stbuf) 
{
	string filename = path;
	string stripped_slash = strip_leading_slash(filename);
	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));

	
	//Attribute des Wurzelverzeichnisses
	if(filename == "/") 
	{
		cout << "ramfs_getattr("<<filename<<"): Returning attributes for /" << endl;
		stbuf->st_mode = S_IFDIR | 0777;
		stbuf->st_nlink = 2;
	} 
	//Eine existierende Datei wird gelesen
	else if(file_exists(stripped_slash)) 
	{
		cout << "ramfs_getattr("<<stripped_slash<<"): Returning attributes" << endl;
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = files[stripped_slash].size();
	}
	//Datei nicht vorhanden
	else 
	{
		cout << "ramfs_getattr("<<stripped_slash<<"): not found" << endl;
		res = -ENOENT;
	}

	return res;
}

/** Liest den Inhalt eines Verzeichnisses aus */
static int ramfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
	//Dateisystem kennt keine Unterverzeichnisse
	if(strcmp(path, "/") != 0)
	{
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
static int ramfs_open(const char *path, struct fuse_file_info *fi)
{
	string filename = strip_leading_slash(path);
	
	//Datei nicht vorhanden
	if( !file_exists(filename) ) 
	{
		cout << "ramfs_readdir("<<filename<<"): Not found" << endl;
		return -ENOENT;
	}

	return 0;
}

/* Liest (Teile einer) Datei */
static int ramfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
	string filename = strip_leading_slash(path);

	//Datei nicht vorhanden
	if( !file_exists(filename) )
	{
		cout << "ramfs_read("<<filename<<"): Not found" << endl;
		return -ENOENT;
	}
	
	//Datei existiert. Lese in Puffer
	map<int, unsigned char> &file = files[filename];
	size_t len = file.size();
	
	//Prüfe, wieviele Bytes ab welchem Offset gelesen werden können
	if (offset < len) 
	{
		if (offset + size > len) 
		{
			cout << "ramfs_read("<<filename<<"): offset("<<offset<<
				") + size("<<size<<") > len("<<len<<"), setting to " << len - offset << endl;
			
			size = len - offset;
		}

		//		
		cout << "ramfs_read("<<filename<<"): Reading "<< size << " bytes"<<endl;
		for(size_t i = 0; i < size; ++i)
			buf[i] = file[offset + i];
	}
	//Offset war groesser als die max. Groesse der Datei
	else 
	{
		return -EINVAL;
	}

	return size;
}

/** Erzeugt ein neues Dateisystemelement */
int ramfs_mknod(const char *path, mode_t mode, dev_t dev) 
{
	string filename = strip_leading_slash(path);
	//Datei bereits vorhanden
	if( file_exists(filename) )
	{
		cout << "ramfs_mknod("<<filename<<"): Already exists" << endl;
		return -EEXIST;
	}
	
	//Datei bereits vorhanden
	if( (mode & S_IFREG) == 0)
	{
		cout << "ramfs_mknod("<<filename<<"): Only files may be created" << endl;
		return -EINVAL;
	}

	cout << "ramfs_mknod("<<filename<<"): Creating empty file" << endl;
	files[filename] = to_map("");
	return 0;
}

/** Schreibt Daten in eine (offene) Datei */
int ramfs_write(const char *path, const char *data, size_t size, off_t offset, struct fuse_file_info *) 
{
	string filename = strip_leading_slash(path);
	
	//Datei nicht vorhanden
	if( !file_exists(filename) )
	{
		cout << "ramfs_write("<<filename<<"): Not found" << endl;
		return -ENOENT;
	}

	//Datei existiert. Lese in Puffer
	map<int, unsigned char> &file = files[filename];
	
	cout << "ramfs_write("<<filename<<"): Writing "<< size << " bytes startting with offset "<< offset<<endl;
	
	for(size_t i = 0; i < size; ++i)
		file[offset + i] = data[i];
		
	return size;
}

/** Löscht eine Datei */
int ramfs_unlink(const char *pathname) 
{
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
	return fuse_main(argc, argv, &ramfs_oper, NULL);
}

}