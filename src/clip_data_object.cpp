//// by fanxiushu 2025-04-17
//// 用于windows平台，实现IDataObject ,IStream 等接口，用于 OLE剪贴板的文件传输，
//// 实现从虚拟端朝windows资源管理器传输复制文件

#include <WinSock2.h>
#include "sddl.h"
#include <string>
#include <assert.h>
#include <WtsApi32.h>
#include <windows.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <codecvt>
#include <shlobj.h> 

#include <string>
#include <vector>
using namespace std;

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "Shlwapi.lib")

class ClipCopyFileDataObject :public IDataObject
{
	friend class ClipCopyFileStream;
public:
	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) {
		if (!ppvObj)return E_INVALIDARG;
		if (riid == IID_IUnknown || riid == IID_IDataObject) {
			*ppvObj = (riid == IID_IUnknown) ? (IUnknown*)this : (IDataObject*)this;
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() {
		return InterlockedIncrement(&RefCount);
	}
	STDMETHODIMP_(ULONG) Release() {
		LONG cnt = InterlockedDecrement(&RefCount);
		if (cnt == 0) {
			delete this;
		}
		return cnt;
	}

	// IDataObject
	STDMETHODIMP GetData(FORMATETC* pfe, STGMEDIUM* pmed);
	STDMETHODIMP GetDataHere(FORMATETC* pfe, STGMEDIUM* pmed) {
		return E_NOTIMPL;
	}
	STDMETHODIMP QueryGetData(FORMATETC* pfe);
	STDMETHODIMP GetCanonicalFormatEtc(FORMATETC* pfeIn,
		FORMATETC* pfeOut)
	{
		*pfeOut = *pfeIn;
		pfeOut->ptd = NULL;
		return DATA_S_SAMEFORMATETC;
	}
	STDMETHODIMP SetData(FORMATETC* pfe, STGMEDIUM* pmed,
		BOOL fRelease) {
		return E_NOTIMPL;
	}
	STDMETHODIMP EnumFormatEtc(DWORD dwDirection,
		LPENUMFORMATETC* ppefe);
	STDMETHODIMP DAdvise(FORMATETC* pfe, DWORD grfAdv,
		IAdviseSink* pAdvSink, DWORD* pdwConnection)
	{
		return OLE_E_ADVISENOTSUPPORTED;
	}
	STDMETHODIMP DUnadvise(DWORD dwConnection)
	{
		return OLE_E_ADVISENOTSUPPORTED;
	}
	STDMETHODIMP EnumDAdvise(LPENUMSTATDATA* ppefe)
	{
		return OLE_E_ADVISENOTSUPPORTED;
	}

	///
	volatile LONG RefCount;
	UINT clipfmt_filedesc;
	UINT clipfmt_filecontent;
	vector<class ClipCopyFileStream*> files;

	///
	IDataObject* pDataObjectShell;
	IDataObject* queryDataObjectShell() {
		if (pDataObjectShell)return pDataObjectShell;
		////
		typedef HRESULT (CALLBACK *fnSHCreateDataObject)(__in PCIDLIST_ABSOLUTE pidlFolder, __in UINT cidl,
			__in_ecount_opt(cidl) PCUITEMID_CHILD_ARRAY apidl,
			__in_opt IDataObject *pdtInner, __in REFIID riid, __deref_out void **ppv);
		static fnSHCreateDataObject fn = (fnSHCreateDataObject)
			GetProcAddress(LoadLibraryA("shell32.dll"), "SHCreateDataObject");

		if (!fn)return NULL;
		HRESULT hr = fn(NULL, NULL, NULL, NULL, IID_IDataObject, (void**)&pDataObjectShell);
		if (FAILED(hr)) {
			pDataObjectShell = NULL;
		}
	//	printf("SHCreateDataObject OK\n");
		return pDataObjectShell;
	}

public:
	ClipCopyFileDataObject();
	~ClipCopyFileDataObject();

public:
	int AddFile(
		const char* utf8_relativePath,int indexPath,
		int64_t fileSize, int64_t mtime,
		int(*read_block)(void* stream_handle,
			char* data, int64_t offset, int len), int maxBlock,
		void* param, void(*free_param)(void* param));
};

class ClipCopyFileStream :public IStream
{
public:
	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) {
		if (!ppvObj)return E_INVALIDARG;
		*ppvObj = NULL;
		if (riid == IID_IUnknown) {
			*ppvObj = (IUnknown*)this;
		}
		else if (riid == IID_ISequentialStream) {
			*ppvObj = (ISequentialStream*)this;
		}
		else if (riid == IID_IStream) {
			*ppvObj = (IStream*)this;
		}
		////
		if (*ppvObj) {
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() {
		return dataobj_mgr->AddRef();
	}
	STDMETHODIMP_(ULONG) Release() {
		return dataobj_mgr->Release();
	}

	/// ISequentialStream
	STDMETHODIMP Read(void *pv, ULONG cb, ULONG *pcbRead);
	STDMETHODIMP Write(const void *pv, ULONG cb, ULONG *pcbWrite)
	{
		return S_OK;
	}
		
	//// IStream 
	STDMETHODIMP Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
	{
		int64_t new_pos = 0;
		switch (dwOrigin)
		{
		case STREAM_SEEK_SET:
			break;
		case STREAM_SEEK_CUR:
			new_pos = curr_pos;
			break;
		case STREAM_SEEK_END:
			new_pos = file_size;
			break;
		default:
			return STG_E_INVALIDFUNCTION;
		}
		////
		new_pos += dlibMove.QuadPart;
		if (new_pos < 0 || new_pos >file_size) {
			return STG_E_INVALIDFUNCTION;
		}
		if (plibNewPosition) {
			plibNewPosition->QuadPart = new_pos;
		}
		curr_pos = new_pos;

		return S_OK;
	}
	STDMETHODIMP SetSize(ULARGE_INTEGER libNewSize) {
		return E_NOTIMPL;
	}
	STDMETHODIMP CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*,
		ULARGE_INTEGER*)
	{
		return E_NOTIMPL;
	}
	STDMETHODIMP Commit(DWORD)
	{
		return E_NOTIMPL;
	}
	STDMETHODIMP Revert(void)
	{
		return E_NOTIMPL;
	}
	STDMETHODIMP LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
	{
		return E_NOTIMPL;
	}
	STDMETHODIMP UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD)
	{
		return E_NOTIMPL;
	}
	STDMETHODIMP Clone(IStream **)
	{
		return E_NOTIMPL;
	}
	STDMETHODIMP Stat(STATSTG *pstatstg, DWORD grfStatFlag)
	{
		if (!pstatstg)return E_INVALIDARG;
		memset(pstatstg, 0, sizeof(STATSTG));
		////
		pstatstg->pwcsName = NULL;
		pstatstg->type = STGTY_STREAM;
		pstatstg->cbSize.QuadPart=file_size;
		return S_OK;
	}
 
public:
	ClipCopyFileDataObject* dataobj_mgr;// dataobj_mgr
	///
	int      stream_index;

	int64_t  curr_pos;
	int64_t  file_size;
	int64_t  mtime;         ///修改时间,1970以来的 秒数 
	string   utf8_relativePath;
	int      indexPath;
	wstring  relativePath;  ///相对路径，比如 a\name.txt

	int(*read_block)(void* stream_handle, char* data, int64_t offset, int len);
	int max_block;
	void*    param;
	void(*free_param)(void* param);

};

///// impl
ClipCopyFileDataObject::ClipCopyFileDataObject(): RefCount(1), pDataObjectShell(NULL)
{
	clipfmt_filedesc = RegisterClipboardFormatW(L"FileGroupDescriptorW"); ///注册成 wchar
	clipfmt_filecontent = RegisterClipboardFormat(CFSTR_FILECONTENTS);
}

ClipCopyFileDataObject::~ClipCopyFileDataObject()
{
	printf("-- ClipCopyFileDataObject::~ClipCopyFileDataObject \n");
	////
	for (int i = 0; i < files.size(); ++i) {
		if (files[i]->free_param) {
			files[i]->free_param(files[i]->param);
		}

		delete files[i];
	}
	files.clear();
	if (pDataObjectShell)pDataObjectShell->Release();
}

HRESULT STDMETHODCALLTYPE ClipCopyFileDataObject::QueryGetData(FORMATETC* pfe)
{
	if (!pfe)return S_FALSE;
	if (pfe->cfFormat == clipfmt_filedesc) {///文件总览
		if ((pfe->tymed&TYMED_HGLOBAL) && pfe->dwAspect == DVASPECT_CONTENT ) return S_OK;
		////
	}
	else if (pfe->cfFormat == clipfmt_filecontent) {//文件内容
		if ((pfe->tymed&TYMED_ISTREAM) && pfe->dwAspect == DVASPECT_CONTENT) {
	//		printf("ClipCopyFileDataObject::QueryGetData()\n");
			if (pfe->lindex >= 0 && pfe->lindex < files.size()) return S_OK;
		}
	}
	else if (queryDataObjectShell()) {
		return pDataObjectShell->QueryGetData(pfe);
	}

	//////
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE ClipCopyFileDataObject::EnumFormatEtc(DWORD dwDirection,
	LPENUMFORMATETC* ppefe)
{
	////
	if (dwDirection != DATADIR_GET) {
		if (ppefe)*ppefe = NULL;
		return E_NOTIMPL;
	}
//	printf("ClipCopyFileDataObject::EnumFormatEtc\n");
	/////
	int cnt = files.size() + 1;
	FORMATETC* arr = (FORMATETC*)malloc( sizeof(FORMATETC)*cnt );
	memset(arr, 0, sizeof(FORMATETC)*cnt);
	arr[0].cfFormat = clipfmt_filedesc;
	arr[0].tymed = TYMED_HGLOBAL;
	arr[0].dwAspect = DVASPECT_CONTENT;
	arr[0].lindex = -1;
	arr[0].ptd = NULL;
	for (int i = 1; i < cnt; ++i) {
		FORMATETC* a = &arr[i];
		a->cfFormat = clipfmt_filecontent;
		a->dwAspect = DVASPECT_CONTENT;
		a->tymed = TYMED_ISTREAM;
		a->ptd = NULL;
		a->lindex = i - 1;
	}

	HRESULT hr = SHCreateStdEnumFmtEtc(cnt, arr, ppefe);
	free(arr);

	return hr;
}

static void usec_time_tToFileTime(int64_t t, LARGE_INTEGER* ft)
{
	LONGLONG ff = ((LONGLONG)t) * 10 + 116444736000000000LL;
	ft->QuadPart = ff;
}
HRESULT STDMETHODCALLTYPE ClipCopyFileDataObject::GetData(FORMATETC* pfe, STGMEDIUM* pmed)
{
	if (!pfe || !pmed)return E_FAIL;
	memset(pmed, 0, sizeof(STGMEDIUM));

	////
	if (pfe->cfFormat == clipfmt_filedesc ) {///所有文件信息
		///
		if ( !(pfe->tymed &TYMED_HGLOBAL) ) {
			return DV_E_FORMATETC;
		}
	//	printf("ClipCopyFileDataObject::GetData\n");
		///
		if (files.size() == 0) {//no files
			return STG_E_NOMOREFILES;
		}

		int size = sizeof(FILEGROUPDESCRIPTORW) + (files.size() - 1) * sizeof(FILEDESCRIPTORW);
		HGLOBAL hgbl = GlobalAlloc(GMEM_MOVEABLE, size);
		if (!hgbl) {
			return E_OUTOFMEMORY;
		}
		FILEGROUPDESCRIPTORW* desc = (FILEGROUPDESCRIPTORW*)GlobalLock(hgbl);
		if (!desc) {
			GlobalFree(hgbl);
			return E_OUTOFMEMORY;
		}

		desc->cItems = files.size();
		for (int i = 0; i < desc->cItems; ++i) {
			FILEDESCRIPTORW* fd = &desc->fgd[i];
			ClipCopyFileStream* s = files[i];
			memset(fd, 0, sizeof(FILEDESCRIPTORW));
			////
			fd->dwFlags = FD_FILESIZE | FD_ATTRIBUTES | FD_WRITESTIME | FD_PROGRESSUI;
			LARGE_INTEGER size; size.QuadPart = s->file_size;
			usec_time_tToFileTime(s->mtime * 1000 * 1000, (LARGE_INTEGER*)&fd->ftLastWriteTime);
			fd->nFileSizeLow = size.LowPart;
			fd->nFileSizeHigh = size.HighPart;
			fd->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
			wcsncpy(fd->cFileName, s->relativePath.c_str(), MAX_PATH);
		}
		GlobalUnlock(hgbl);

		/////
		pmed->tymed = TYMED_HGLOBAL;
		pmed->hGlobal = hgbl;

		return S_OK;
	}
	else if (pfe->cfFormat == clipfmt_filecontent) {/// file content
		///
		if (!(pfe->tymed&TYMED_ISTREAM)) {
			return DV_E_FORMATETC;
		}
	//	printf("ClipCopyFileDataObject::GetData content\n");
		/////
		if (pfe->lindex < 0 || pfe->lindex >= files.size()) {
			return DV_E_FORMATETC;
		}

		ClipCopyFileStream* s = files[pfe->lindex];
		s->curr_pos = 0;// 重置 pos

		pmed->tymed = TYMED_ISTREAM;
		pmed->pstm = (IStream*)s;
		s->AddRef();

		return S_OK;

	}
	else if (queryDataObjectShell()) {
		return pDataObjectShell->GetData(pfe, pmed);
	}

	/////
	return DV_E_FORMATETC;
}

int ClipCopyFileDataObject::AddFile(
	const char* utf8_relativePath, int indexPath,
	int64_t fileSize, int64_t mtime,
	int(*read_block)(void* stream_handle,
		char* data, int64_t offset, int len), int maxBlock,
	void* param, void(*free_param)(void* param))
{
	WCHAR wname[8192]=L"";
	MultiByteToWideChar(CP_UTF8, 0, utf8_relativePath, -1, wname, 8000); wname[8000] = 0;

	ClipCopyFileStream* s = new ClipCopyFileStream;
	s->dataobj_mgr = this;
	s->utf8_relativePath = utf8_relativePath; //单纯保存，用来在回调函数中识别具体的文件
	s->indexPath = indexPath;                 //
	s->relativePath = wname;
	s->file_size = fileSize;
	s->curr_pos = 0;
	s->mtime = mtime;
	s->read_block = read_block;
	s->max_block = maxBlock; if (s->max_block < 4096)s->max_block = 4096;
	s->param = param;
	s->free_param = free_param;
	s->stream_index = files.size(); ////

	files.push_back(s);

	return 0;
}

//// IStream
HRESULT STDMETHODCALLTYPE ClipCopyFileStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
{
	int len = min(file_size - curr_pos, cb);
	int pos = 0;
	while (pos < len ) {
		int64_t off = curr_pos + pos;
		int dlen = min(this->max_block, len - pos);
		int r = read_block(this, (char*)pv + pos, off, dlen);
		if (r < 0) {// error
			return STG_E_INVALIDPOINTER; ///
		}
		if (r == 0)break;
		///
		pos += r;
	}
	curr_pos += pos; ///
	if (pcbRead)*pcbRead = pos;

	return S_FALSE;
}

///// interface
extern "C" 
void* clip_copyfile_create()
{
	ClipCopyFileDataObject* obj = new ClipCopyFileDataObject;
	return obj;
}

extern "C"
int clip_copyfile_addfile(void* handle, 
	const char* utf8_relativePath, int indexPath,
	int64_t fileSize, int64_t mtime,
	int(*read_block)(void* stream_handle, 
		char* data, int64_t offset, int len), int maxBlock,
	void* param, void(*free_param)(void* param))
{
	ClipCopyFileDataObject* obj = (ClipCopyFileDataObject*)handle;
	if (!obj)return -1;
	///
	return obj->AddFile(utf8_relativePath, indexPath,
		fileSize, mtime, read_block, maxBlock, param, free_param);
}

extern "C"
void clip_copyfile_stream_fileInfo(
	void* stream_handle,int* p_streamIndex,
	const char** p_relativePath, int* p_indexPath,
	int64_t* p_size, int64_t* p_mtime)
{
	ClipCopyFileStream* s = (ClipCopyFileStream*)stream_handle;
	if (p_streamIndex)*p_streamIndex = s->stream_index;
	if (p_relativePath)*p_relativePath = s->utf8_relativePath.c_str();
	if (p_indexPath)*p_indexPath = s->indexPath;
	if (p_size)*p_size = s->file_size;
	if (p_mtime)*p_mtime = s->mtime;
}

extern "C" 
void* clip_copyfile_stream_param(void* stream_handle)
{
	return ((ClipCopyFileStream*)stream_handle)->param;
}

extern "C" 
int clip_copyfile_set_clipboard(void* handle)
{
	ClipCopyFileDataObject* obj = (ClipCopyFileDataObject*)handle;
	if (!obj)return -1;
	///
	if (obj->files.size() == 0) {///没有文件，
		obj->Release();
		return -1;
	}

	//////
	HRESULT hr = S_OK;
	for (int i = 0; i < 3; ++i) {
		hr = OleSetClipboard(obj);
		if (hr == S_OK)break;
	}
	obj->Release();

	if (hr != S_OK) {
		printf("*** OleSetClipboard Err=0x%X\n", hr );
	}
	return (hr == S_OK) ? 0 : -1;
}

//////////////

