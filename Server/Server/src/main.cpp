#include "server/Server.h"

#include <Windows.h>
#include <tchar.h>

SERVICE_STATUS        serviceStatus = { 0 };
SERVICE_STATUS_HANDLE statusHandle = NULL;
HANDLE                serviceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI serviceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI serviceCtrlHandler(DWORD);
void serviceWorkerThread();

#define SERVICE_NAME  _T("ServerService")

int _tmain(int argc, TCHAR *argv[])
{
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{ SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)serviceMain },
		{ NULL, NULL }
	};

	if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
	{
		return GetLastError();
	}
	return 0;
}


VOID WINAPI serviceMain(DWORD argc, LPTSTR *argv)
{
	DWORD Status = E_FAIL;

	statusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, serviceCtrlHandler);

	if (statusHandle == NULL)
	{
		return;
	}

	ZeroMemory(&serviceStatus, sizeof (serviceStatus));
	serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	serviceStatus.dwControlsAccepted = 0;
	serviceStatus.dwCurrentState = SERVICE_START_PENDING;
	serviceStatus.dwWin32ExitCode = 0;
	serviceStatus.dwServiceSpecificExitCode = 0;
	serviceStatus.dwCheckPoint = 0;

	SetServiceStatus(statusHandle, &serviceStatus);

	serviceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (serviceStopEvent == NULL)
	{
		serviceStatus.dwControlsAccepted = 0;
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		serviceStatus.dwWin32ExitCode = GetLastError();
		serviceStatus.dwCheckPoint = 1;

		SetServiceStatus(statusHandle, &serviceStatus);
		return;
	}

	serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	serviceStatus.dwCurrentState = SERVICE_RUNNING;
	serviceStatus.dwWin32ExitCode = 0;
	serviceStatus.dwCheckPoint = 0;

	SetServiceStatus(statusHandle, &serviceStatus);

	std::thread th(serviceWorkerThread);
	th.join();

	CloseHandle(serviceStopEvent);

	serviceStatus.dwControlsAccepted = 0;
	serviceStatus.dwCurrentState = SERVICE_STOPPED;
	serviceStatus.dwWin32ExitCode = 0;
	serviceStatus.dwCheckPoint = 3;

	SetServiceStatus(statusHandle, &serviceStatus);
}


VOID WINAPI serviceCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:

		if (serviceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		serviceStatus.dwControlsAccepted = 0;
		serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		serviceStatus.dwWin32ExitCode = 0;
		serviceStatus.dwCheckPoint = 4;

		SetServiceStatus(statusHandle, &serviceStatus);

		SetEvent(serviceStopEvent);

		break;

	default:
		break;
	}
}

void serviceWorkerThread()
{
	Server s(serviceStopEvent);
}
