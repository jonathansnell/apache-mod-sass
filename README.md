# mod_sass

mod_sass is Sass handler module for Apache HTTPD Server.

## Dependencies

* [libsass](https://github.com/hcatlin/libsass/) (Release 3.3.4)

## Build

    git clone https://github.com/jonathansnell/apache-mod-sass.git apache-mod-sass
    cd apache-mod-sass
    git clone --branch 3.3.4 https://github.com/sass/libsass.git libsass
    cd libsass
    make
    sudo make install
    cd ../
    make
    sudo make install

## Configuration

`sass.conf` (On Debian: `/etc/apache2/mods-available/sass.conf`):

    # Handler sass script
    AddHandler sass-script .css
    AddHandler sass-script .map

    # Output to CSS/source map file [On | Off]
    SassSaveOutput Off

    # Display error [On | Off]
    DisplayError Off

    # Output style [Expanded | Nested | Compact | Compressed]
    SassOutputStyle Nested

    # If you want inline source comments [On | Off]
    SassSourceComments Off

    # Generate a source map [On | Off]
    SassSourceMap Off

    # Disable sourceMappingUrl in css output [On | Off]
    SassOmitSourceMapUrl Off

    # Embed sourceMappingUrl as data uri [On | Off]
    SassSourceMapEmbed Off

    # Embed include contents in maps [On | Off]
    SassSourceMapContents Off

    # Pass-through as sourceRoot property
    SassSourceMapRoot path/to/src

    # Colon-separated list include of paths; Semicolon-separated on Windows
    SassIncludePaths path/to/inc

    # Colon-separated list plugin of paths; Semicolon-separated on Windows
    SassIncludePaths path/to/plugins

    # Precision for outputting fractional numbers
    SassPrecision 5

## Example

`example.scss`:

```sass
// Variables
$blue: #3bbfce;
$margin: 16px;

.content-navigation {
  border-color: $blue;
  color:
    darken($blue, 9%);
}

.border {
  padding: $margin / 2;
  margin: $margin / 2;
  border-color: $blue;
}

// Nesting
table.hl {
  margin: 2em 0;
  td.ln {
    text-align: right;
  }
}

li {
  font: {
    family: serif;
    weight: bold;
    size: 1.2em;
  }
}

// Mixins
@mixin table-base {
  th {
    text-align: center;
    font-weight: bold;
  }
  td, th {padding: 2px}
}

@mixin left($dist) {
  float: left;
  margin-left: $dist;
}

#data {
  @include left(10px);
  @include table-base;
}

// Selector Inheritance
.error {
  border: 1px #f00;
  background: #fdd;
}
.error.intrusion {
  font-size: 1.3em;
  font-weight: bold;
}

.badError {
  @extend .error;
  border-width: 3px;
}
```

**Output:**

```css
.content-navigation {
  border-color: #3bbfce;
  color: #2ca2af; }

.border {
  padding: 8px;
  margin: 8px;
  border-color: #3bbfce; }

table.hl {
  margin: 2em 0; }
  table.hl td.ln {
    text-align: right; }

li {
  font-family: serif;
  font-weight: bold;
  font-size: 1.2em; }

#data {
  float: left;
  margin-left: 10px; }
  #data th {
    text-align: center;
    font-weight: bold; }
  #data td, #data th {
    padding: 2px; }

.error, .badError {
  border: 1px #f00;
  background: #fdd; }

.error.intrusion, .intrusion.badError {
  font-size: 1.3em;
  font-weight: bold; }

.badError {
  border-width: 3px; }
```
