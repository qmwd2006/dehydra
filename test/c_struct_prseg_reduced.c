/* Reduced from mozilla-central/nsprpub/pr/src/memory/prseg.c */

/* GCC 4.5.0 with cc1 defines a tree such that TYPE_LANG_SPECIFIC
   holds but LANG_TYPE_IS_CLASS does not. */

struct _IO_FILE {
  int _flags;
  char* _IO_read_ptr;
  char* _IO_read_end;
  char* _IO_read_base;
  char* _IO_write_base;
  char* _IO_write_ptr;
  char* _IO_write_end;
  char* _IO_buf_base;
  char* _IO_buf_end;
  char *_IO_save_base;
  char *_IO_backup_base;
  char *_IO_save_end;
  struct _IO_marker *_markers;
  void* _offset;
  void *__pad1;
  void *__pad2;
};
