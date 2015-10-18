EXPORT_FUNC(get_version)
EXPORT_FUNC(getc)
EXPORT_FUNC(tstc)
EXPORT_FUNC(putc)
EXPORT_FUNC(puts)
EXPORT_FUNC(printf)
EXPORT_FUNC(install_hdlr)
EXPORT_FUNC(free_hdlr)
EXPORT_FUNC(malloc)
EXPORT_FUNC(free)
EXPORT_FUNC(udelay)
EXPORT_FUNC(get_timer)
EXPORT_FUNC(vprintf)
EXPORT_FUNC(do_reset)
EXPORT_FUNC(getenv)
EXPORT_FUNC(setenv)
#ifdef CONFIG_HAS_UID
EXPORT_FUNC(forceenv)
#endif
EXPORT_FUNC(simple_strtoul)
EXPORT_FUNC(simple_strtol)
EXPORT_FUNC(strcmp)
#if defined(CONFIG_CMD_I2C)
EXPORT_FUNC(i2c_write)
EXPORT_FUNC(i2c_read)
#endif
