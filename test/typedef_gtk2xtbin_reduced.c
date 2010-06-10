// Reduced from mozilla-central/widget/src/gtkxtbin/gtk2xtbin.c

typedef struct _GError GError;
struct _GError
{
};

typedef struct _GObjectClass GObjectClass;
struct _GObjectClass
{
};


typedef struct _GOutputStream GOutputStream;
typedef struct _GOutputStreamClass GOutputStreamClass;

struct _GOutputStream
{
};

struct _GOutputStreamClass
{
  GObjectClass parent_class;
  void (* write_fn) (GOutputStream *stream,
                                 GError **error);
  void (* splice) (GOutputStream *stream,
                                 GError **error);
  void (* flush) (GOutputStream *stream,
                                 GError **error);
  void (* close_fn) (GOutputStream *stream,
                                 void* user_data);
  void (* flush_finish) (GOutputStream *stream,
                                 GError **error);
  void (* close_async) (GOutputStream *stream,
                                 void* user_data);
  void (* close_finish) (GOutputStream *stream,
                                 GError **error);
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
  void (*_g_reserved6) (void);
  void (*_g_reserved7) (void);
  void (*_g_reserved8) (void);
};

struct _GFilterOutputStream
{
  GOutputStreamClass parent_class;
};
