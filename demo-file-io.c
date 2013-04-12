#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define BUFFERSIZE 100

int main() {
	char* 	dateiname = "testdir/testdatei.txt";
	char* 	text = "Lorem ipsum dolor sit amet, consectetuer adipiscing...\n";
	char 	buffer[BUFFERSIZE];

	//---------------------------------------------------------
	// Datei erzeugen und Daten schreiben
	//---------------------------------------------------------
	
		//Erzeuge eine neue Datei zum Schreiben (S_IWRITE)
		int file_handle = creat(dateiname, S_IWRITE);
		
		//Schreibe den Text in die Datei
		int byte_geschrieben = write(file_handle, text, strlen(text));
		printf("%d byte in Datei %s geschrieben\n", byte_geschrieben, dateiname);
		
		//Schliesse die Datei (Filehandle freigeben)
		close(file_handle);

	//---------------------------------------------------------
	// Datei öffnen und Daten lesen
	//---------------------------------------------------------
	
		//Oeffne die Datei zum Lesen (neues Filehandle)
		file_handle = open(dateiname, O_RDONLY); 
	
		//Lese max. BUFFERSIZE-1 bytes aus der Datei in den Puffer
		int bytes_read = read(file_handle, &buffer, BUFFERSIZE-1);
		
		//Sicherstellen, dass letztes Zeichen ein \0 ist und ausgeben
		buffer[bytes_read] = '\0';
		printf("Aus Datei %s gelesen: %s", dateiname, buffer);
	
		//Schliesse die Datei (Filehandle freigeben)
		close(file_handle);
}