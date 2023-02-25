/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Windows-specific initialization
 */

#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <ufprog/log.h>
#include <ufprog/dirs.h>
#include <ufprog/misc.h>
#include "win32.h"

#define PATH_INCREMENT			1024

typedef HRESULT (STDAPICALLTYPE *TSHGetKnownFolderPath)(_In_ REFKNOWNFOLDERID rfid,
							_In_ DWORD /* KNOWN_FOLDER_FLAG */ dwFlags,
							_In_opt_ HANDLE hToken,
							_Outptr_ PWSTR *ppszPath);

static ctrlc_handler ctrlc_cb;
static LARGE_INTEGER qpc_freq;
static char *progname;

static ufprog_bool os_register_app_dirs(void)
{
	WCHAR szOldAppdataPath[MAX_PATH], *pszAppData = NULL;
	char *appdata_base, *new_path;
	ufprog_status ret;
	HRESULT hret;

	if (IsWindowsVistaOrGreater()) {
		TSHGetKnownFolderPath fnSHGetKnownFolderPath;
		HMODULE hShell32;

		hShell32 = GetModuleHandle(_T("shell32.dll"));
		if (!hShell32) {
			hShell32 = LoadLibrary(_T("shell32.dll"));
			if (!hShell32) {
				log_err("Failed to load shell32.dll\n");
				return false;
			}
		}

		fnSHGetKnownFolderPath = (TSHGetKnownFolderPath)(uintptr_t)GetProcAddress(hShell32,
											  "SHGetKnownFolderPath");
		if (!fnSHGetKnownFolderPath) {
			log_err("Failed to locate SHGetKnownFolderPath in shell32.dll\n");
			return false;
		}

		hret = fnSHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &pszAppData);
	} else {
		hret = SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, szOldAppdataPath);
		if (hret == S_OK)
			pszAppData = szOldAppdataPath;
	}

	if (hret != S_OK || !pszAppData) {
		log_err("Failed to get user's application data directory\n");
		if (IsWindowsVistaOrGreater() && pszAppData)
			CoTaskMemFree(pszAppData);
		return false;
	}

	appdata_base = wcs_to_utf8(pszAppData);
	if (IsWindowsVistaOrGreater())
		CoTaskMemFree(pszAppData);

	if (!appdata_base) {
		log_err("Failed to convert user's application data directory to UTF-8\n");
		return false;
	}

	/* Register program's config directory */
	new_path = path_concat(true, 0, appdata_base, "", UFPROG_APPDATA_NAME, NULL);
	if (!new_path) {
		log_err("Failed to generate program's config directory\n");
		goto cleanup;
	}

	if (!os_mkdir_p(new_path)) {
		log_err("Failed to create program's config directory\n");
		goto cleanup;
	}

	ret = add_dir(DIR_CONFIG, new_path);
	free(new_path);

	if (ret)
		goto cleanup;

	/* Register program's device directory */
	new_path = path_concat(true, 0, appdata_base, "", UFPROG_APPDATA_NAME, UFPROG_DEVICE_DIR_NAME, NULL);
	if (!new_path) {
		log_err("Failed to generate program's device directory\n");
		goto cleanup;
	}

	ret = add_dir(DIR_DEVICE, new_path);
	free(new_path);

	if (ret)
		goto cleanup;

	/* Register program's controller directory */
	new_path = path_concat(true, 0, appdata_base, "", UFPROG_APPDATA_NAME, UFPROG_INTERFACE_DIR_NAME, NULL);
	if (!new_path) {
		log_err("Failed to generate program's controller directory\n");
		goto cleanup;
	}

	ret = add_dir(DIR_DRIVER, new_path);
	free(new_path);

	if (ret)
		goto cleanup;

	return true;

cleanup:
	free(appdata_base);
	return false;
}

static ufprog_bool os_register_default_dirs(void)
{
	WCHAR *szProgPath = NULL, *szProgPathNew, *szPtr, *szDot;
	DWORD dwBuffSize = 0, dwSize;
	ufprog_status ret;
	char *utf8_str;

	/* Get program's full path first */
	while (true) {
		dwBuffSize += PATH_INCREMENT;

		if (!szProgPath)
			szProgPathNew = malloc(dwBuffSize * sizeof(WCHAR));
		else
			szProgPathNew = realloc(szProgPath, dwBuffSize * sizeof(WCHAR));

		if (!szProgPathNew) {
			log_err("No memory for program's root directory\n");
			if (szProgPath)
				free(szProgPath);
			return false;
		}

		szProgPath = szProgPathNew;
		dwSize = GetModuleFileNameW(NULL, szProgPath, dwBuffSize);

		if (dwSize < dwBuffSize) {
			szProgPath[dwSize] = 0;
			break;
		}
	}

	/* Split program's root directory */
	szPtr = szProgPath + wcslen(szProgPath) - 1;
	szDot = szPtr;

	while (szPtr >= szProgPath) {
		if (*szPtr == '/' || *szPtr == '\\') {
			szPtr++;
			break;
		}

		szPtr--;
	}

	if (szPtr <= szProgPath) {
		log_err("Failed to parse program's root directory\n");
		free(szProgPath);
		return false;
	}

	/* Store program's name without extension name */
	while (szDot >= szPtr) {
		if (*szDot == '.') {
			*szDot = 0;
			break;
		}

		szDot--;
	}

	progname = wcs_to_utf8(szPtr);
	if (!progname) {
		log_err("Failed to convert program name to UTF-8\n");
		free(szProgPath);
		return false;
	}

	/* Register program's root directory */
	*szPtr = 0;

	utf8_str = wcs_to_utf8(szProgPath);
	free(szProgPath);
	if (!utf8_str) {
		log_err("Failed to convert root directory to UTF-8\n");
		return false;
	}

	ret = set_root_dir(utf8_str);
	if (ret) {
		free(utf8_str);
		return false;
	}

	/* Register program's data directory */
	ret = add_dir(DIR_DATA_ROOT, utf8_str);
	if (ret) {
		free(utf8_str);
		return false;
	}

	if (uses_portable_dirs()) {
		/* Register program's config directory */
		ret = add_dir(DIR_CONFIG, utf8_str);
		free(utf8_str);

		if (ret)
			return false;
	} else {
		free(utf8_str);
	}

	/* Register program's devices directory */
	utf8_str = path_concat(true, 0, get_root_dir(), UFPROG_DEVICE_DIR_NAME, NULL);
	if (!utf8_str) {
		log_err("Failed to generate program's device directory\n");
		return false;
	}

	ret = add_dir(DIR_DEVICE, utf8_str);
	free(utf8_str);

	if (ret)
		return false;

	/* Register program's drivers directory */
	utf8_str = path_concat(true, 0, get_root_dir(), UFPROG_INTERFACE_DIR_NAME, NULL);
	if (!utf8_str) {
		log_err("Failed to generate program's controller directory\n");
		return false;
	}

	ret = add_dir(DIR_DRIVER, utf8_str);
	free(utf8_str);

	if (ret)
		return false;

	return true;
}

ufprog_bool UFPROG_API os_init(void)
{
	_tsetlocale(LC_CTYPE, _T(""));

	if (!QueryPerformanceFrequency(&qpc_freq))
		return false;

	if (!uses_portable_dirs())
		os_register_app_dirs();

	os_register_default_dirs();

	return true;
}

const char *UFPROG_API os_prog_name(void)
{
	return progname;
}

ufprog_status UFPROG_API os_vfprintf(FILE *f, const char *fmt, va_list args)
{
	ufprog_status ret = UFP_OK;
	LPWSTR lpwsText = NULL;
	char *buf = NULL;
	HANDLE hStdCon;
	int len;

	if (!fmt)
		return UFP_INVALID_PARAMETER;

	len = vsnprintf(NULL, 0, fmt, args);
	if (len < 0) {
		ret = UFP_FAIL;
		goto out;
	}

	buf = malloc(len + 1);
	if (!buf) {
		ret = UFP_NOMEM;
		goto out;
	}

	len = vsnprintf(buf, len + 1, fmt, args);
	if (len < 0) {
		ret = UFP_FAIL;
		goto out;
	}

	/* Convert text to UTF-16 */
	lpwsText = utf8_to_wcs(buf);
	if (!lpwsText) {
		ret = UFP_FAIL;
		goto out;
	}

	if (f == stderr)
		hStdCon = GetStdHandle(STD_ERROR_HANDLE);
	else
		hStdCon = GetStdHandle(STD_OUTPUT_HANDLE);

	WriteConsoleW(hStdCon, lpwsText, lstrlenW(lpwsText), NULL, NULL);

out:
	if (lpwsText)
		free(lpwsText);

	if (buf)
		free(buf);

	return ret;
}

ufprog_status UFPROG_API os_fprintf(FILE *f, const char *fmt, ...)
{
	ufprog_status ret;
	va_list args;

	if (!fmt)
		return UFP_INVALID_PARAMETER;

	va_start(args, fmt);
	ret = os_vfprintf(f, fmt, args);
	va_end(args);

	return ret;
}

char *UFPROG_API os_getline_alloc(FILE *f)
{
	LPWSTR lpwsText = NULL;
	size_t len = 0;
	char *utf8str;

	wgetdelim(&lpwsText, &len, '\n', f);

	utf8str = wcs_to_utf8(lpwsText);
	free(lpwsText);

	return utf8str;
}

ufprog_status UFPROG_API os_load_module(const char *module_path, module_handle *handle)
{
	PWSTR pwsModule;
	HMODULE hModule;

	if (!module_path || !handle)
		return UFP_INVALID_PARAMETER;

	pwsModule = utf8_to_wcs(module_path);
	if (!pwsModule)
		return UFP_NOMEM;

	hModule = LoadLibraryW(pwsModule);
	free(pwsModule);

	if (!hModule) {
		if (GetLastError() == ERROR_MOD_NOT_FOUND)
			return UFP_FILE_NOT_EXIST;

		return UFP_FAIL;
	}

	*handle = (module_handle)hModule;

	return UFP_OK;
}

void UFPROG_API os_unload_module(module_handle module)
{
	if (!module)
		return;

	FreeLibrary((HMODULE)module);
}

void *UFPROG_API os_find_module_symbol(module_handle module, const char *name)
{
	if (!module || !name)
		return NULL;

	return (void *)(uintptr_t)GetProcAddress((HMODULE)module, name);
}

ufprog_status UFPROG_API os_find_module_symbols(module_handle module, struct symbol_find_entry *list, size_t count,
						ufprog_bool full)
{
	ufprog_bool missing = false;
	FARPROC proc;
	size_t i;

	if (!module || !list)
		return UFP_INVALID_PARAMETER;

	if (!count)
		return UFP_OK;

	for (i = 0; i < count; i++) {
		proc = GetProcAddress((HMODULE)module, list[i].name);
		if (proc)
			list[i].found = true;
		else
			missing = true;

		if (list[i].psymbol)
			*list[i].psymbol = (void *)(uintptr_t)proc;
	}

	if (full && missing)
		return UFP_FAIL;

	return UFP_OK;
}

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	return ctrlc_cb();
}

ufprog_bool UFPROG_API os_register_ctrlc_handler(ctrlc_handler handler)
{
	DWORD dwErrorCode;

	ctrlc_cb = handler;

	if (!SetConsoleCtrlHandler(CtrlHandler, handler ? TRUE : FALSE)) {
		dwErrorCode = GetLastError();
		log_sys_error_utf8(dwErrorCode, "Failed to %sregister Ctrl-C handler", handler ? "" : "de");
		return false;
	}

	return true;
}

ufprog_bool UFPROG_API os_create_mutex(mutex_handle *outmutex)
{
	HANDLE hMutex;

	if (!outmutex)
		return false;

	hMutex = CreateMutexW(NULL, FALSE, NULL);
	if (hMutex == INVALID_HANDLE_VALUE) {
		*outmutex = NULL;
		return false;
	}

	*outmutex = (mutex_handle)hMutex;
	return true;
}

ufprog_bool UFPROG_API os_free_mutex(mutex_handle mutex)
{
	if (!mutex)
		return false;

	return CloseHandle((HANDLE)mutex);
}

ufprog_bool UFPROG_API os_mutex_lock(mutex_handle mutex)
{
	DWORD dwResult;

	if (!mutex)
		return false;

	dwResult = WaitForSingleObject((HANDLE)mutex, INFINITE);
	if (dwResult == WAIT_OBJECT_0)
		return true;

	return false;
}

ufprog_bool UFPROG_API os_mutex_unlock(mutex_handle mutex)
{
	if (!mutex)
		return false;

	return ReleaseMutex((HANDLE)mutex);
}

uint64_t UFPROG_API os_get_timer_us(void)
{
	LARGE_INTEGER t;

	if (!QueryPerformanceCounter(&t))
		return 0;

	return t.QuadPart * 1000000 / qpc_freq.QuadPart;
}

void UFPROG_API os_udelay(uint64_t us)
{
	LARGE_INTEGER t, te;

	if (!QueryPerformanceCounter(&t))
		return;

	te.QuadPart = t.QuadPart + (qpc_freq.QuadPart * us + 500000) / 1000000;

	do {
		if (!QueryPerformanceCounter(&t))
			return;
	} while (t.QuadPart <= te.QuadPart);
}

ufprog_status UFPROG_API os_read_text_file(const char *filename, char **outdata, size_t *retlen)
{
	char *rawdata, *utf8_data;
	size_t datasize, i;
	ufprog_status ret;

	ret = read_file_contents(filename, (void **)&rawdata, &datasize);
	if (ret) {
		*outdata = NULL;

		if (retlen)
			*retlen = 0;

		return ret;
	}

	if (((uint8_t)rawdata[0] == 0xff && (uint8_t)rawdata[1] == 0xfe) ||
	    ((uint8_t)rawdata[0] == 0xfe && (uint8_t)rawdata[1] == 0xff)) {
		/* Convert UCS-2 to UTF-8 */

		if ((uint8_t)rawdata[0] == 0xfe && (uint8_t)rawdata[1] == 0xff) {
			/* Big-endian to Little-endian */
			for (i = 0; i < datasize; i += 2) {
				char tmp = rawdata[i];
				rawdata[i] = rawdata[i + 1];
				rawdata[i + 1] = tmp;
			}
		}

		utf8_data = wcs_to_utf8((PWSTR)(rawdata + 2));
		free(rawdata);

		if (!utf8_data) {
			*outdata = NULL;

			if (retlen)
				*retlen = 0;

			return UFP_NOMEM;
		}

		*outdata = utf8_data;

		if (retlen)
			*retlen = strlen(utf8_data);

		return UFP_OK;
	}

	if ((uint8_t)rawdata[0] == 0xef && (uint8_t)rawdata[1] == 0xbb && (uint8_t)rawdata[2] == 0xbf) {
		/* Remove UTF-8 BOM */
		memmove(rawdata, rawdata + 3, datasize - 3);
		datasize -= 3;
	}

	/* Assume text without BOM is UTF-8 ... */
	*outdata = rawdata;

	if (retlen)
		*retlen = datasize;

	return UFP_OK;
}

/*
 * This function is used in default log printing.
 * To avoid infinite nested calls, do not print logs here.
 */
PWSTR utf8_to_wcs(PCSTR pInputText)
{
	PWSTR pwsOutputText;
	int len;

	if (!pInputText)
		return NULL;

	len = MultiByteToWideChar(CP_UTF8, 0, pInputText, -1, NULL, 0);
	if (!len)
		return L"";

	pwsOutputText = malloc(sizeof(WCHAR) * len);
	if (!pwsOutputText)
		return L"";

	len = MultiByteToWideChar(CP_UTF8, 0, pInputText, -1, pwsOutputText, len);
	if (!len) {
		free(pwsOutputText);
		return L"";
	}

	return pwsOutputText;
}

char *wcs_to_utf8(PCWSTR pInputText)
{
	char *utf8_str;
	int len;

	if (!pInputText)
		return NULL;

	len = WideCharToMultiByte(CP_UTF8, 0, pInputText, -1, NULL, 0, NULL, NULL);
	if (!len)
		return "";

	utf8_str = malloc(len);
	if (!utf8_str)
		return "";

	len = WideCharToMultiByte(CP_UTF8, 0, pInputText, -1, utf8_str, len, NULL, NULL);
	if (!len) {
		free(utf8_str);
		return "";
	}

	return utf8_str;
}

PWSTR get_system_error_va(DWORD dwErrorCode, UINT nNumArgs, va_list args)
{
	PWSTR p, pwsOriginalMessage, pwsFullMessage = NULL, *pArgs = NULL;
	UINT nNumInserts = 0, i;
	DWORD dwSize;

	if (!FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			    NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (PWSTR)&pwsOriginalMessage,
			    0, NULL))
		return NULL;

	/* Count inserts */
	p = pwsOriginalMessage;
	while (*p) {
		if (*p == '%') {
			if (isdigit(p[1])) {
				p++;
				while (*p && isdigit(*p))
					p++;

				nNumInserts++;
				continue;
			}
		}

		p++;
	}

	/* Create argument array */
	if (nNumInserts) {
		pArgs = calloc(nNumInserts, sizeof(PWSTR));
		if (!pArgs)
			goto out;

		for (i = 0; i < nNumInserts; i++) {
			if (i < nNumArgs) {
				pArgs[i] = va_arg(args, PWSTR);
			} else {
				/* Although it's impossible, we still allocate enough space for UINT16_MAX */
				pArgs[i] = malloc(8);
				if (pArgs[i])
					StringCbPrintfW(pArgs[i], 8, L"%%%u", i + 1);
			}
		}
	}

	dwSize = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY,
		NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (PWSTR)&pwsFullMessage, 0,
		(va_list *)pArgs);

	if (!dwSize) {
		pwsFullMessage = NULL;
	} else {
		p = pwsFullMessage + lstrlenW(pwsFullMessage);

		while (p > pwsFullMessage) {
			if (p[-1] == '\r' || p[-1] == '\n')
				p[-1] = 0;
			else
				break;

			p--;
		}
	}

out:
	if (pwsOriginalMessage)
		LocalFree(pwsOriginalMessage);

	if (pArgs) {
		for (i = 0; i < nNumInserts; i++) {
			if (i >= nNumArgs) {
				if (pArgs[i])
					free(pArgs[i]);
			}
		}

		free(pArgs);
	}

	return pwsFullMessage;
}

PWSTR get_system_error(DWORD dwErrorCode, UINT nNumArgs, ...)
{
	PWSTR pwsErrorMessage;
	va_list args;

	va_start(args, nNumArgs);

	pwsErrorMessage = get_system_error_va(dwErrorCode, nNumArgs, args);

	va_end(args);

	return pwsErrorMessage;
}

char *get_system_error_utf8(DWORD dwErrorCode, UINT nNumArgs, ...)
{
	PWSTR pwsErrorMessage;
	char *utf8_str = NULL;
	va_list args;

	va_start(args, nNumArgs);

	pwsErrorMessage = get_system_error_va(dwErrorCode, nNumArgs, args);
	if (pwsErrorMessage) {
		utf8_str = wcs_to_utf8(pwsErrorMessage);
		LocalFree(pwsErrorMessage);
	}

	va_end(args);

	return utf8_str;
}

void log_sys_error_utf8(DWORD dwErrorCode, const char *fmt, ...)
{
	char *logtext = NULL, *syserr = NULL;
	va_list args;
	int len;

	if (!fmt)
		return;

	va_start(args, fmt);

	len = vsnprintf(NULL, 0, fmt, args);
	if (len < 0)
		goto out;

	logtext = malloc(len + 1);
	if (!logtext)
		goto out;

	len = vsnprintf(logtext, len + 1, fmt, args);
	if (len < 0)
		goto out;

	syserr = get_system_error_utf8(dwErrorCode, 0);

	if (syserr)
		log_err("%s: %s\n", logtext, syserr);
	else
		log_err("%s: error %u\n", logtext, dwErrorCode);

out:
	va_end(args);

	if (logtext)
		free(logtext);

	if (syserr)
		free(syserr);
}
