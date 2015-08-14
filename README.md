# mod_sass

mod_sass is Sass handler module for Apache HTTPD Server.

## Dependencies

* [libsass](https://github.com/hcatlin/libsass/) (Release 3.2.5)

## Build

    git clone https://github.com/jonathansnell/apache-mod-sass.git apache-mod-sass
    cd apache-mod-sass
    git clone --branch 3.2.5 https://github.com/sass/libsass.git libsass
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

    # Output style [Expanded | Nested | Compact | Compressed]
    SassOutputStyle Nested

    # Output to CSS file [On | Off]
    SassOutput Off

    # Display error [On | Off]
    DisplayError Off

    # Include paths (optimal) [PATH]
    SassIncludePaths path/to/inc

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
