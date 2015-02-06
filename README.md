# mod_sass

mod_sass is Sass handler module for Apache HTTPD Server.
Tested on Debian/GNU 8 (Testing / Sid) with Apache 2.4.10

## Dependencies

* [libsass](https://github.com/hcatlin/libsass/) (Release 3.1.0)

## Build

    git clone git@github.com:EggieCode/apache-mod-sass.git apache-mod-sass
    cd apache-mod-sass
    git clone --branch 3.1.0 git@github.com:sass/libsass.git libsass
    cd libsass
    make
    sudo make install
    cd ../
    make
    sudo make install


## Configration

sass.conf (On Debian: /etc/apache2/mods-available/sass.conf):

    # Handler sass script
    AddHandler sass-script .scss

    # Output compressed (minify) [On | Off]
    SassCompressed On

    # Output to CSS file [On | Off]
    SassOutput Off

    # Include paths (optimal) [PATH]
    SassIncludePaths path/to/inc

    # Image Path (optimal) [PATH]
    SassImagePath path/to/img

## Example

example.scss:

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

**Output:**

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
