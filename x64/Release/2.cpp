#include<Windows.h>

int main()
{
	HWND hwnd=FindWindowW(L"#32770", L"将打印输出另存为");
	
	PostMessage(hwnd,WM_CLOSE,0,0);
	return 0;
}