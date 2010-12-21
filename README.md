# Drush extra commands

 These are a bunch of extra commands for
 [drush](http://drupal.org/project/drush "Drush project page") that
 I've created. 
 
 In order to use them you must:
 
 1. Clone the repository to a given directory.
 
 2. Either add the following to your `~/.drush/drushrc.php`
      
    `$options['include'] = '/path/to/extra_drush_commands';`
 
    or pass the option `-i=/path/to/extra_drush_commands` to drush.
   
 3. You should be ready and set to go. Verify that everything is
    working by issuing: `drush help sql-secure-dump`, for example. You
    should get the help text for `sql-secure-dump`.
   

## 1. drush sql-secure-dump

   This command implements a **secure** database dump. It uses the
   `openssl` command line utility to encrypt and decrypt the files.

    drush sql-secure-dump @mysite

   This creates a gzipped and encrypted dump of the Drupal database
   for the site configured under the **alias** mysite on your
   `~/.drush` directory.

   By default the dumps are stored in `~/.drush-dumps`. If the
   directory doesn't exist it's created by the command. You can
   override the default dump destination with the 
   `--result-file=/path/to/secure_dump` option.

   The dump is encrypted using the
   [Triple DES](http://en.wikipedia.org/wiki/Triple_DES "3DES at Wikipedia") encryption algorithm.

   The command assumes that the password is stored in
   `~/.drush-dumps/.dump-passwd`. A different password file can be
   specified through the option
   `--passwd-file=/path/to/passwd_file`. If no password file is given
   or found it asks for the password.

   The command tries to guess where the `openssl` binary is located
   using the GNU utility `which`. This can be overriden specifying the
   command line option `--openssl=/path/to/openssl`.

   To decrypt the encoded dump use the command:
   
     openssl des3 -d -in /path/to/encrypted_dump.enc -out /path/to/decrypted_dump -pass file:/path/to/passwd_file
     
   If not using a password file then just issue:
   
     openssl des3 -d -in /path/to/encrypted_dump.enc -out /path/to/decrypted_dump
     
   The password will be requested.
   
   The command uses the default gzip compression level. Which is
   **6**. This can be overriden by specifying the option
   `--compression-level=<compression level>` where `<compression
   level>` is a number between 1 and 9.
   
   The dump created has the filename `<database name>-<hostname>-YearMonthDay-HourMinutesSeconds.sql.gz.enc`.
   
   Example: `my_drupal_db-localhost_2010Dec19-164017.sql.gz.enc`
   
   To get the dump to skip the _common_ tables like `cache_*` and such
   add the following to your `~/.drush/drushrc.php` file:
   
       // Always drop the common tables when doing a dump.
       $command_specific['sql-secure-dump']['skip-tables-key'] = 'common';
   
   Of course this is not a _paranoid_ version of a DB dump. There are
   many potential flaws in the process. If you really want a **as
   secure as possible** setup then nothing beats **full disk
   encryption**.
   
   This command is useful for exchanging dumps out in the open
   network without fearing someone prying in the dump
   content. Suppose you're providing a dump for a client or for
   someone else. No longer you have to sanitize the dump removing the
   email addresses from the `users` table.
   
   As usual in `drush`, command specific help is obtained through
   `drush help sql-secure-dump`.
   
   The command **alias** is `sqsec`.
   
## 2. drush sql-gz-dump

   This is just a version of the `sql-dump` command that provides a
   gzipped dump.    
 
     drush sql-gz-dump @my-site 
 
   This creates a gzipped dump of the Drupal database
   for the site configured under the **alias** mysite on your
   `~/.drush=` directory.

   By default the dumps are stored in `~/.drush-dumps`. If the
   directory doesn't exist it's created by the command. You can
   override the default dump destination with the 
   `--result-file=/path/to/gzipped_dump` option.

   The command uses the default gzip compression level. Which is
   **6**. This can be overriden by specifying the option
   `--compression-level=<compression level>` where `<compression
   level>` is a number between 1 and 9.

   The dump created has the filename `<database name>-<hostname>-YearMonthDay-HourMinutesSeconds.sql.gz`.
   
   Example: `my_drupal_db-localhost_2010Dec19-164017.sql.gz`

   To get the dump to skip the _common_ tables like `cache_*` and such
   add the following to your `~/.drush/drushrc.php` file:
   
       // Always drop the common tables when doing a dump.
       $command_specific['sql-gz-dump']['skip-tables-key'] = 'common';



   As usual in `drush`, command specific help is obtained through
   `drush help sql-gz-dump`.

   The command **alias** is `sqgz`.
