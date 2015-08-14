/*
**  mod_sass.c -- Apache sass module
**
**  Then activate it in Apache's httpd.conf file:
**
**    # httpd.conf
**    LoadModule sass_module modules/mod_sass.so
**    <IfModule sass_module>
**      AddHandler sass-script .css
**      AddHandler sass-script .map
**      SassOutputStyle  Nested (Expanded | Nested | Compact | Compressed)
**      SassOutput       Off (On | Off)
**      SassDisplayError Off (On | Off)
**      SassIncludePaths path/to/sass
**    </IfModule sass_module>
*/

#include <libgen.h>

/* httpd */
#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "ap_config.h"
#include "apr_strings.h"

/* libsass */
#include "libsass/sass_interface.h"

#ifdef HAVE_CONFIG_H
#  undef PACKAGE_NAME
#  undef PACKAGE_STRING
#  undef PACKAGE_TARNAME
#  undef PACKAGE_VERSION
#  include "config.h"
#endif

/* log */
#ifdef AP_SASS_DEBUG_LOG_LEVEL
#define SASS_DEBUG_LOG_LEVEL AP_SASS_DEBUG_LOG_LEVEL
#else
#define SASS_DEBUG_LOG_LEVEL APLOG_DEBUG
#endif

#define _RERR(r, format, args...)                                       \
    ap_log_rerror(APLOG_MARK, APLOG_CRIT, 0,                            \
                  r, "[SASS] %s(%d): "format, __FILE__, __LINE__, ##args)
#define _SERR(s, format, args...)                                       \
    ap_log_error(APLOG_MARK, APLOG_CRIT, 0,                             \
                 s, "[SASS] %s(%d): "format, __FILE__, __LINE__, ##args)
#define _PERR(p, format, args...)                                       \
    ap_log_perror(APLOG_MARK, APLOG_CRIT, 0,                            \
                  p, "[SASS] %s(%d): "format, __FILE__, __LINE__, ##args)

#define _RDEBUG(r, format, args...)                                     \
    ap_log_rerror(APLOG_MARK, SASS_DEBUG_LOG_LEVEL, 0,                  \
                  r, "[SASS_DEBUG] %s(%d): "format, __FILE__, __LINE__, ##args)
#define _SDEBUG(s, format, args...)                                     \
    ap_log_error(APLOG_MARK, SASS_DEBUG_LOG_LEVEL, 0,                   \
                 s, "[SASS_DEBUG] %s(%d): "format, __FILE__, __LINE__, ##args)
#define _PDEBUG(p, format, args...)                                     \
    ap_log_perror(APLOG_MARK, SASS_DEBUG_LOG_LEVEL, 0,                  \
                  p, "[SASS_DEBUG] %s(%d): "format, __FILE__, __LINE__, ##args)

/* content types */
#define SASS_CONTENT_TYPE_CSS        "text/css"
#define SASS_CONTENT_TYPE_SOURCE_MAP "application/json"
#define SASS_CONTENT_TYPE_ERROR      "text/plain"

/* sass dir config */
typedef struct {
    int is_output;
    int display_error;
    char *output_style;
    char *include_paths;
} sass_dir_config_t;

module AP_MODULE_DECLARE_DATA sass_module;

// Stackoverflow script to get filename extention
const char *get_filename_ext(const char *filename) {
	const char *dot = strrchr(filename, '.');
	if(!dot || dot == filename) return NULL;
		return dot;
}

int exists(const char *filename) {
    FILE *f;
    if (f = fopen(filename, "r")) {
        fclose(f);
        return 1;
    }
    return 0;
}

/* output css file */
static void
sass_output_file(request_rec *r, char *data)
{
    char *fbase, *fname;
    apr_status_t rc;
    apr_size_t bytes;
    apr_file_t *file = NULL;

    const char *ext = get_filename_ext(r->filename);

    if (!data || !ext) {
        return;
    }

    fbase = apr_pstrndup(r->pool, r->filename, ext - r->filename);
    fname = apr_psprintf(r->pool, "%s.css", fbase);

    rc = apr_file_open(&file, fname, APR_WRITE | APR_CREATE,
                       APR_UREAD | APR_UWRITE | APR_GREAD, r->pool);
    if (rc == APR_SUCCESS) {
        bytes = strlen(data);
        rc = apr_file_write(file, data, &bytes);
        if (rc != APR_SUCCESS) {
            _RERR(r, "Can't create/write to file: %s", fname);
        }
        apr_file_close(file);
    } else {
        _RERR(r, "Can't create/write to file: %s", fname);
    }
}

/* content handler */
static int
sass_handler(request_rec *r)
{
    int retval = OK;
    sass_dir_config_t *config;
    struct sass_file_context *context;
    char *css_name, *map_name, *scss_name;
    char *uri_base, *css_uri, *map_uri;
    int is_css = 0;
    int is_map = 0;

    const char *ext = get_filename_ext(r->filename);
    const char *fbase = apr_pstrndup(r->pool, r->filename, ext - r->filename);

    if (apr_strnatcasecmp(r->handler, "sass-script") != 0) {
        return DECLINED;
    }

    if (apr_strnatcasecmp(ext, ".css") == 0) {
        is_css = 1;
    } else if (apr_strnatcasecmp(ext, ".map") == 0) {
        is_map = 1;
    }

    if (!is_css && !is_map) {
        return DECLINED;
    }

    css_name  = apr_psprintf(r->pool, "%s.css",  fbase);
    map_name  = apr_psprintf(r->pool, "%s.map",  fbase);
    scss_name = apr_psprintf(r->pool, "%s.scss", fbase);
    uri_base  = dirname(r->uri);
    css_uri   = apr_psprintf(r->pool, "%s/%s", uri_base, basename(css_name));
    map_uri   = apr_psprintf(r->pool, "%s/%s", uri_base, basename(map_name));

    if (!exists(scss_name)) {
        return DECLINED;
    }

    if (M_GET != r->method_number) {
        return HTTP_METHOD_NOT_ALLOWED;
    }

    config = ap_get_module_config(r->per_dir_config, &sass_module);
    context = sass_new_file_context();
    if (!context) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    context->options.include_paths = config->include_paths;
    if (apr_strnatcasecmp(config->output_style, "expanded") == 0) {
        context->options.output_style = SASS_STYLE_EXPANDED;
    }
    else if (apr_strnatcasecmp(config->output_style, "compact") == 0) {
        context->options.output_style = SASS_STYLE_COMPACT;
    }
    else if (apr_strnatcasecmp(config->output_style, "compressed") == 0) {
        context->options.output_style = SASS_STYLE_COMPRESSED;
    } else {
        context->options.output_style = SASS_STYLE_NESTED;
    }
    context->options.source_map_file = map_uri;
    context->options.source_comments = 0;
    context->options.source_map_contents = 1;
    context->input_path = scss_name;
    context->output_path = css_uri;

    sass_compile_file(context);

    if (context->error_status) {
        r->content_type = SASS_CONTENT_TYPE_ERROR;
        if (context->error_message) {
            ap_rprintf(r, "%s", context->error_message);
        } else {
            ap_rputs("An error occured; no error message available.", r);
        }
        if (!config->display_error) {
            retval = HTTP_INTERNAL_SERVER_ERROR;
        }
    } else if (is_map && context->source_map_string) {
        r->content_type = SASS_CONTENT_TYPE_SOURCE_MAP;
        ap_rprintf(r, "%s", context->source_map_string);
    } else if (context->output_string) {
        r->content_type = SASS_CONTENT_TYPE_CSS;
        ap_rprintf(r, "%s", context->output_string);
        if (config->is_output) {
            sass_output_file(r, context->output_string);
        }
    } else {
        r->content_type = SASS_CONTENT_TYPE_ERROR;
        ap_rputs("Unknown internal error.", r);
        if (!config->display_error) {
            retval = HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    sass_free_file_context(context);

    return retval;
}

/* create dir config */
static void *
sass_create_dir_config(apr_pool_t *p, char *dir)
{
    sass_dir_config_t *config = apr_pcalloc(p, sizeof(sass_dir_config_t));

    if (config) {
        memset(config, 0, sizeof(sass_dir_config_t));

        config->is_output = 0;
        config->display_error = 0;
        config->output_style = "";
        config->include_paths = "";
    }

    return (void *)config;
}

/* merge dir config */
static void *
sass_merge_dir_config(apr_pool_t *p, void *base_conf, void *override_conf)
{
    sass_dir_config_t *config =
        (sass_dir_config_t *)apr_pcalloc(p, sizeof(sass_dir_config_t));
    sass_dir_config_t *base = (sass_dir_config_t *)base_conf;
    sass_dir_config_t *override = (sass_dir_config_t *)override_conf;


    if (override->is_output != 0) {
        config->is_output = 1;
    } else {
        config->is_output = base->is_output;
    }

    if (override->display_error != 0) {
        config->display_error = 1;
    } else {
        config->display_error = base->display_error;
    }

    if (override->output_style && strlen(override->output_style) > 0) {
        config->output_style = override->output_style;
    } else {
        config->output_style = base->output_style;
    }
    if (override->include_paths && strlen(override->include_paths) > 0) {
        config->include_paths = override->include_paths;
    } else {
        config->include_paths = base->include_paths;
    }


    return (void *)config;
}

/* Commands */
static const command_rec sass_cmds[] =
{
    AP_INIT_FLAG("SassOutput", ap_set_flag_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, is_output),
                 RSRC_CONF|ACCESS_CONF, "sass output (css) to 'on' or 'off'"),
    AP_INIT_FLAG("SassDisplayError", ap_set_flag_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, display_error),
                 RSRC_CONF|ACCESS_CONF, "sass display error to 'on' or 'off'"),
    AP_INIT_TAKE1("SassOutputStyle", ap_set_string_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, output_style),
                 RSRC_CONF|ACCESS_CONF, "sass output style"),
    AP_INIT_TAKE1("SassIncludePaths", ap_set_string_slot,
                  (void *)APR_OFFSETOF(sass_dir_config_t, include_paths),
                  RSRC_CONF|ACCESS_CONF, "sass include paths"),
    { NULL }
};


/* Hooks */
static void sass_register_hooks(apr_pool_t *p)
{
    ap_hook_handler(sass_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Module */
module AP_MODULE_DECLARE_DATA sass_module =
{
    STANDARD20_MODULE_STUFF,
    sass_create_dir_config,  /* create per-dir    config structures */
    sass_merge_dir_config,   /* merge  per-dir    config structures */
    NULL,                    /* create per-server config structures */
    NULL,                    /* merge  per-server config structures */
    sass_cmds,               /* table of config file commands       */
    sass_register_hooks      /* register hooks                      */
};
