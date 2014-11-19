#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <errno.h>
#include <WinSock2.h>
#include <Windows.h>

#include <thread>
#include <memory>
#include <vector>

#define PORT 8888
#define BUF_SIZE 256
#define FILE_BUF_SIZE 1024
#define ROOT_PATH "."

//

char* httpVersion = "HTTP/1.1";

char* dateHeader = "Date: Tue, 18 Nov 2014 00:00:00 GMT";

char* status200 = "200 OK";
char* status400 = "400 Bad Request";
char* status404 = "404 Not Found";

char* body400 = "<h1>400 Bad Request</h1>";
char* body404 = "<h1>404 Not Found</h1>";

//

void WorkerThread(SOCKET s);

void PrintError(const int errorCode);

int Send(const SOCKET s, char* const buf, const int len);
int Recv(const SOCKET s, char* const buf, const int len);

void main()
{
	WSAData wsadata = { 0, };
	SOCKET listenSocket = INVALID_SOCKET;
	sockaddr_in serverAddr = { 0, };

	puts("Starting server...");

	if (WSAStartup(MAKEWORD(2, 2), &wsadata))
	{
		PrintError(WSAGetLastError());
		goto exit;
	}

	listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSocket == INVALID_SOCKET)
	{
		PrintError(WSAGetLastError());
		goto wsaclean;
	}

	char option = 1;
	if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == SOCKET_ERROR)
	{
		PrintError(WSAGetLastError());
		goto closesock;
	}

	serverAddr.sin_family = PF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(PORT);
	if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		PrintError(WSAGetLastError());
		goto closesock;
	}

	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		PrintError(WSAGetLastError());
		goto closesock;
	}

	puts("Server started.");

	while (true)
	{
		sockaddr_in clientAddr;
		int addrLen = sizeof(clientAddr);
		SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET)
		{
			PrintError(WSAGetLastError());
			continue;
		}
		puts("Client connected");
		std::thread(WorkerThread, clientSocket).detach();
	}

closesock:
	closesocket(listenSocket);

wsaclean:
	WSACleanup();

exit:
	return;
}

void WorkerThread(SOCKET s)
{
	int result;
	int recvBytes = 0;
	char buf[BUF_SIZE] = { 0 };
	char header[BUF_SIZE] = { 0 };
	char path[BUF_SIZE] = { 0 };
	char filebuf[FILE_BUF_SIZE];

	do
	{
		result = recv(s, buf, BUF_SIZE, 0);
		if (result == SOCKET_ERROR)
		{
			PrintError(WSAGetLastError());
			return;
		}
		else if (result == 0)
			break;

		for (int i = 0; i < result - 1; ++i)
		{
			if (buf[recvBytes + i] == '\r' && buf[recvBytes + i + 1] == '\n')
			{
				recvBytes = recvBytes + i + 2;
				goto loopbreak;
			}
		}

		recvBytes += result;

	} while (true);

	if (recvBytes == 0)
	{
		closesocket(s);
		return;
	}

loopbreak:
	memcpy_s(header, recvBytes + 1, buf, recvBytes);
	header[recvBytes] = '\0';
	puts(header);

	char* context = nullptr;
	char* token = nullptr;
	char* delim = " ";

	do
	{
		token = strtok_s(header, delim, &context);
		token = strtok_s(nullptr, delim, &context);
		if (!token)
		{
			// 400
			sprintf_s(buf, "%s %s\r\n%s\r\nContent-Length:%d\r\n\r\n%s", httpVersion, status400, dateHeader, strlen(body400), body400);
			puts(buf);
			Send(s, buf, strlen(buf));
			break;
		}
		sprintf_s(path, "%s%s", ROOT_PATH, token);

		FILE* file;
		if (fopen_s(&file, path, "rb"))
		{
			perror("ERROR");
			if (errno == ENOENT)
			{
				// 404
				sprintf_s(buf, "%s %s\r\n%s\r\nContent-Length:%d\r\n\r\n%s", httpVersion, status404, dateHeader, strlen(body404), body404);
				puts(buf);
				Send(s, buf, strlen(buf));
			}
			break;
		}

		fseek(file, 0, SEEK_END);
		int size = ftell(file);
		fseek(file, 0, SEEK_SET);

		// 200
		sprintf_s(buf, "%s %s\r\n%s\r\nContent-Length:%d\r\n\r\n", httpVersion, status200, dateHeader, size);
		puts(buf);
		Send(s, buf, strlen(buf));

		do
		{
			int readBytes = fread_s(filebuf, FILE_BUF_SIZE, 1, BUF_SIZE, file);
			Send(s, filebuf, readBytes);
		} while (!feof(file));

		fclose(file);

	} while (false);

	Sleep(10);
	
	closesocket(s);
}

void PrintError(const int errorCode)
{
	char* buf = nullptr;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		nullptr,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&buf,
		0,
		nullptr);
	fputs("ERROR: ", stderr);
	fputs(buf, stderr);
}

int Send(const SOCKET s, char* const buf, const int len)
{
	int result;
	int sentBytes = 0;
	while (sentBytes < len)
	{
		result = send(s, buf + sentBytes, len - sentBytes, 0);
		if (result == SOCKET_ERROR)
			return result;
		else if (result == 0)
			break;
		sentBytes += result;
	}
	return sentBytes;
}

int Recv(const SOCKET s, char* const buf, const int len)
{
	int result;
	int recvBytes = 0;
	while (recvBytes < len)
	{
		result = recv(s, buf + recvBytes, len - recvBytes, 0);
		if (result == SOCKET_ERROR)
			return result;
		else if (result == 0)
			break;
		recvBytes += result;
	}
	return recvBytes;
}
