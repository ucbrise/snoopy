namespace debug_log
{
extern const char *NAME;
void set_name(const char *name);
void info(const char *format, ...);
void error(const char *format, ...);
} // namespace log
