/*
**  mod_sass.c -- Apache sass module
**
**  Then activate it in Apache's httpd.conf file:
**
**    # httpd.conf
**    LoadModule sass_module modules/mod_sass.so
**    <IfModule sass_module>
**      AddHandler            sass-script .css
**      AddHandler            sass-script .map
**      SassSaveOutput        Off (On | Off)
**      SassDisplayError      Off (On | Off)
**      SassOutputStyle       Nested (Expanded | Nested | Compact | Compressed)
**      SassSourceComments    Off (On | Off)
**      SassSourceMap         Off (On | Off)
**      SassOmitSourceMapUrl  Off (On | Off)
**      SassSourceMapEmbed    Off (On | Off)
**      SassSourceMapContents Off (On | Off)
**      SassSourceMapRoot     Pass-through as sourceRoot property
**      SassIncludePaths      Colon-separated list of include paths; Semicolon-separated on Windows
**      SassPluginPaths       Colon-separated list of plugin paths; Semicolon-separated on Windows
**      SassPrecision         Precision for outputting fractional numbers
**    </IfModule sass_module>
*/

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
    int save_output;
    int display_error;
    char *output_style;
    int source_comments;
    int source_map;
    int omit_source_map_url;
    int source_map_embed;
    int source_map_contents;
    char *source_map_root;
    char *include_paths;
    char *plugin_paths;
    int precision;
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
sass_output_file(request_rec *r, char *filename, char *data)
{
    apr_status_t rc;
    apr_size_t bytes;
    apr_file_t *file = NULL;

    if (!data) {
        return;
    }

    rc = apr_file_open(&file, filename,
                       APR_FOPEN_WRITE | APR_FOPEN_CREATE | APR_FOPEN_TRUNCATE,
                       APR_FPROT_OS_DEFAULT, r->pool);
    if (rc == APR_SUCCESS) {
        bytes = strlen(data);
        rc = apr_file_write(file, data, &bytes);
        if (rc != APR_SUCCESS) {
            _RERR(r, "Can't create/write to file: %s", filename);
        }
        apr_file_close(file);
    } else {
        _RERR(r, "Can't create/write to file: %s", filename);
    }
}

/* content handler */
static int
sass_handler(request_rec *r)
{
    int retval = OK;
    sass_dir_config_t *config;
    struct sass_file_context *context;
    char *css_name, *map_name, *sass_name, *scss_name;
    int is_css = 0;
    int is_map = 0;
    int is_sass = 0;

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
    sass_name = apr_psprintf(r->pool, "%s.sass", fbase);
    scss_name = apr_psprintf(r->pool, "%s.scss", fbase);

    if (exists(sass_name)) {
        is_sass = 1;
    }

    if (!is_sass && !exists(scss_name)) {
        return DECLINED;
    }

    if (M_GET != r->method_number) {
        return HTTP_METHOD_NOT_ALLOWED;
    }

    config = ap_get_module_config(r->per_dir_config, &sass_module);

    if (config->source_map < 1 && is_map) {
        return DECLINED;
    }

    context = sass_new_file_context();
    if (!context) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

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
    context->options.source_comments = (config->source_comments > 0);
    context->options.omit_source_map_url = (config->omit_source_map_url > 0);
    context->options.source_map_embed = (config->source_map_embed > 0);
    context->options.source_map_contents = (config->source_map_contents > 0);
    context->options.source_map_root = config->source_map_root;
    context->options.include_paths = config->include_paths;
    context->options.plugin_paths = config->plugin_paths;
    if (config->precision > 0) {
        context->options.precision = config->precision;
    }

    context->options.is_indented_syntax_src = is_sass;
    if (is_sass) {
        context->input_path = sass_name;
    } else {
        context->input_path = scss_name;
    }

    if (config->source_map > 0) {
        context->options.source_map_file = map_name;
    }
    context->output_path = css_name;

    sass_compile_file(context);

    if (context->error_status) {
        r->content_type = SASS_CONTENT_TYPE_ERROR;
        if (context->error_message) {
            ap_rprintf(r, "%s", context->error_message);
        } else {
            ap_rputs("An error occured; no error message available.", r);
        }
        if (config->display_error < 1) {
            retval = HTTP_INTERNAL_SERVER_ERROR;
        }
    } else if (is_map && context->source_map_string) {
        r->content_type = SASS_CONTENT_TYPE_SOURCE_MAP;
        ap_rprintf(r, "%s", context->source_map_string);
        if (config->save_output > 0) {
            sass_output_file(r, map_name, context->source_map_string);
            if (context->output_string) {
                sass_output_file(r, css_name, context->output_string);
            }
        }
    } else if (context->output_string) {
        r->content_type = SASS_CONTENT_TYPE_CSS;
        ap_rprintf(r, "%s", context->output_string);
        if (config->save_output > 0) {
            if ((config->source_map > 0) && context->source_map_string) {
                sass_output_file(r, map_name, context->source_map_string);
            }
            sass_output_file(r, css_name, context->output_string);
        }
    } else {
        r->content_type = SASS_CONTENT_TYPE_ERROR;
        ap_rputs("Unknown internal error.", r);
        if (config->display_error < 1) {
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

        config->save_output = -1;
        config->display_error = -1;
        config->output_style = "";
        config->source_comments = -1;
        config->source_map = -1;
        config->omit_source_map_url = -1;
        config->source_map_embed = -1;
        config->source_map_contents = -1;
        config->source_map_root = "";
        config->include_paths = "";
        config->plugin_paths = "";
        config->precision = 0;
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

    config->save_output         = (override->save_output > -1) ?
                                   override->save_output :
                                   base->save_output;

    config->display_error       = (override->display_error > -1) ?
                                   override->display_error :
                                   base->display_error;

    config->output_style        = strlen(override->output_style) ?
                                   override->output_style :
                                   base->output_style;

    config->source_comments     = (override->source_comments > -1) ?
                                   override->source_comments :
                                   base->source_comments;

    config->source_map          = (override->source_map > -1) ?
                                   override->source_map :
                                   base->source_map;

    config->omit_source_map_url = (override->omit_source_map_url > -1) ?
                                   override->omit_source_map_url :
                                   base->omit_source_map_url;

    config->source_map_embed    = (override->source_map_embed > -1) ?
                                   override->source_map_embed :
                                   base->source_map_embed;

    config->source_map_contents = (override->source_map_contents > -1) ?
                                   override->source_map_contents :
                                   base->source_map_contents;

    config->source_map_root     = strlen(override->source_map_root) ?
                                   override->source_map_root :
                                   base->source_map_root;

    config->include_paths       = strlen(override->include_paths) ?
                                   override->include_paths :
                                   base->include_paths;

    config->plugin_paths        = strlen(override->plugin_paths) ?
                                   override->plugin_paths :
                                   base->plugin_paths;

    config->precision           = (override->precision > 0) ?
                                   override->precision :
                                   base->precision;

    return (void *)config;
}

/* Commands */
static const command_rec sass_cmds[] =
{
    AP_INIT_FLAG("SassSaveOutput", ap_set_flag_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, save_output),
                 RSRC_CONF|ACCESS_CONF, "Save CSS/source map output to file"),
    AP_INIT_FLAG("SassDisplayError", ap_set_flag_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, display_error),
                 RSRC_CONF|ACCESS_CONF, "Display errors in the browser"),
    AP_INIT_TAKE1("SassOutputStyle", ap_set_string_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, output_style),
                 RSRC_CONF|ACCESS_CONF, "Output style for the generated css code (Expanded | Nested | Compact | Compressed)"),
    AP_INIT_FLAG("SassSourceComments", ap_set_flag_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, source_comments),
                 RSRC_CONF|ACCESS_CONF, "If you want inline source comments"),
    AP_INIT_FLAG("SassSourceMap", ap_set_flag_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, source_map),
                 RSRC_CONF|ACCESS_CONF, "Generate a source map"),
    AP_INIT_FLAG("SassOmitSourceMapUrl", ap_set_flag_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, omit_source_map_url),
                 RSRC_CONF|ACCESS_CONF, "Disable sourceMappingUrl in css output"),
    AP_INIT_FLAG("SassSourceMapEmbed", ap_set_flag_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, source_map_embed),
                 RSRC_CONF|ACCESS_CONF, "Embed sourceMappingUrl as data uri"),
    AP_INIT_FLAG("SassSourceMapContents", ap_set_flag_slot,
                 (void *)APR_OFFSETOF(sass_dir_config_t, source_map_contents),
                 RSRC_CONF|ACCESS_CONF, "Embed include contents in maps"),
    AP_INIT_TAKE1("SassSourceMapRoot", ap_set_string_slot,
                  (void *)APR_OFFSETOF(sass_dir_config_t, source_map_root),
                  RSRC_CONF|ACCESS_CONF, "Pass-through as sourceRoot property"),
    AP_INIT_TAKE1("SassIncludePaths", ap_set_string_slot,
                  (void *)APR_OFFSETOF(sass_dir_config_t, include_paths),
                  RSRC_CONF|ACCESS_CONF, "Colon-separated list include of paths; Semicolon-separated on Windows"),
    AP_INIT_TAKE1("SassPluginPaths", ap_set_string_slot,
                  (void *)APR_OFFSETOF(sass_dir_config_t, plugin_paths),
                  RSRC_CONF|ACCESS_CONF, "Colon-separated list plugin of paths; Semicolon-separated on Windows"),
    AP_INIT_TAKE1("SassPrecision", ap_set_int_slot,
                  (void *)APR_OFFSETOF(sass_dir_config_t, precision),
                  RSRC_CONF|ACCESS_CONF, "Precision for outputting fractional numbers"),
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
