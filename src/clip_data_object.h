//// by fanxiushu 2025-05-08
#pragma  once

#ifdef __cplusplus
extern "C" {
#endif
	////创建容器，返回句柄，用于下面的函数
	void* clip_copyfile_create();

	////朝容器中添加文件，参数比较多，utf8_relativePath是utf8编码，相对路径或文件名，比如 path\a.txt,或 a.tat
	////indexPath是文件序号，自己对多个文件排序，防止发送混淆，fileSize， mtime分别是文件大小和修改时间戳
	////read_block 是回调函数，当windows资源管理发送粘贴文件的时候，此函数被调用，我们可在此函数中传递每个文件的内容
	////param是额外参数，free_param用于释放额外参数
	int clip_copyfile_addfile(void* handle,
		const char* utf8_relativePath, int indexPath,
		int64_t fileSize, int64_t mtime,
		int(*read_block)(void* stream_handle,
			char* data, int64_t offset, int len), int maxBlock,
		void* param, void(*free_param)(void* param));

	////获取当前传输的流里边的文件信息，只能使用到 read_block 回调函数中，
	void clip_copyfile_stream_fileInfo(
		void* stream_handle, int* p_streamIndex,
		const char** p_relativePath, int* p_indexPath,
		int64_t* p_size, int64_t* p_mtime);

	///获取额外参数，只能使用到 read_block 回调函数中，
	void* clip_copyfile_stream_param(void* stream_handle);

	////当我们添加好所有需要粘贴的文件之后，就可以调用此函数“复制”我们添加的所有文件到 剪切板了。
	int clip_copyfile_set_clipboard(void* handle);

#ifdef __cplusplus
}
#endif
