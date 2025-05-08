//// by fanxiushu 2025-05-08
#pragma  once

#ifdef __cplusplus
extern "C" {
#endif
	////�������������ؾ������������ĺ���
	void* clip_copyfile_create();

	////������������ļ��������Ƚ϶࣬utf8_relativePath��utf8���룬���·�����ļ��������� path\a.txt,�� a.tat
	////indexPath���ļ���ţ��Լ��Զ���ļ����򣬷�ֹ���ͻ�����fileSize�� mtime�ֱ����ļ���С���޸�ʱ���
	////read_block �ǻص���������windows��Դ������ճ���ļ���ʱ�򣬴˺��������ã����ǿ��ڴ˺����д���ÿ���ļ�������
	////param�Ƕ��������free_param�����ͷŶ������
	int clip_copyfile_addfile(void* handle,
		const char* utf8_relativePath, int indexPath,
		int64_t fileSize, int64_t mtime,
		int(*read_block)(void* stream_handle,
			char* data, int64_t offset, int len), int maxBlock,
		void* param, void(*free_param)(void* param));

	////��ȡ��ǰ���������ߵ��ļ���Ϣ��ֻ��ʹ�õ� read_block �ص������У�
	void clip_copyfile_stream_fileInfo(
		void* stream_handle, int* p_streamIndex,
		const char** p_relativePath, int* p_indexPath,
		int64_t* p_size, int64_t* p_mtime);

	///��ȡ���������ֻ��ʹ�õ� read_block �ص������У�
	void* clip_copyfile_stream_param(void* stream_handle);

	////��������Ӻ�������Ҫճ�����ļ�֮�󣬾Ϳ��Ե��ô˺��������ơ�������ӵ������ļ��� ���а��ˡ�
	int clip_copyfile_set_clipboard(void* handle);

#ifdef __cplusplus
}
#endif
