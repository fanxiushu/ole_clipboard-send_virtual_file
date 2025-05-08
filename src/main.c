////fanxiushu 2025-05-08 , example for ole clipboard
#include <Windows.h>
#include <stdio.h>
#include <inttypes.h>

#include "clip_data_object.h"

////
static int read_block(void* strm, char* data, int64_t offset, int len)
{
	int64_t size; const char* name; int fileIndex;
	clip_copyfile_stream_fileInfo(strm, NULL, &name, &fileIndex, &size, NULL);
	printf("-- [%s] index=%d, fileSize=%lld, -- read off=%lld, len=%d\n", name, fileIndex, size, offset, len);

	///������ļ����ݣ������������������
	memset(data, 'X', len);

	return len;
}

int main(int argc, const char* argv[])
{
	OleInitialize(NULL);
	///
	void* h = clip_copyfile_create();

	clip_copyfile_addfile(h, "a\\abc.txt", 0, 121 * 1001, time(0) - 90000, read_block, 64 * 1024, NULL, NULL);
	clip_copyfile_addfile(h, "b\\ff.txt", 1, 12 * 1001, time(0) - 77777, read_block, 64 * 1024, NULL, NULL);

	clip_copyfile_set_clipboard(h);

	printf("��������ļ���ɣ�\n������windows����Դ�������ճ�����ļ���\n");
	////
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

