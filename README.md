# Drush SQL Extras

## Introduction

These are a bunch of [drush](http://drupal.org/project/drush) commands
for working databases. They build on the SQL related
[commands](http://drush.ws) provided by core drush.

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
   
   You can automate this decryption process using a tool like [decrypt-file](https://github.com/perusio/encrypt-file).
   
   The command tries to find a file named `~/.dump-passwd` where the
   password is written. This is the default password filename. 
   
   Given a password file the command tries to change its mode to
   `0400`, meaning to be readable by the owner only. If it cannot fix
   the filemode then it signals that failure as a warning and proceeds.
   
   The command uses the default bzip compression level. Which is
   **1**. This can be overriden by specifying the option
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
   
## 2. drush sql-bz-dump

   This is just a version of the `sql-dump` command that provides a
   bzipped dump.    
 
     drush sql-bz-dump @my-site 
 
   This creates a bzipped dump of the Drupal database
   for the site configured under the **alias** mysite on your
   `~/.drush=` directory.

   By default the dumps are stored in `~/.drush-dumps`. If the
   directory doesn't exist it's created by the command. You can
   override the default dump destination with the 
   `--result-file=/path/to/bzipped_dump` option.

   The command uses the default bzip compression level. Which is
   **1**. This can be overriden by specifying the option
   `--compression-level=<compression level>` where `<compression
   level>` is a number between 1 and 9.

   The dump created has the filename `<database name>-<hostname>-YearMonthDay-HourMinutesSeconds.sql.bz`.
   
   Example: `my_drupal_db-localhost_2010Dec19-164017.sql.bz`

   To get the dump to skip the _common_ tables like `cache_*` and such
   add the following to your `~/.drush/drushrc.php` file:
   
       // Always drop the common tables when doing a dump.
       $command_specific['sql-bz-dump']['skip-tables-key'] = 'common';



   As usual in `drush`, command specific help is obtained through
   `drush help sql-bz-dump`.

   The command **alias** is `sqbz`.

## Parallel compression

The compression commands always try first to use the parallel version
to take advantage of modern multi-processor, multi-core machines. In
the case of bzip that's [`pbzip2`](http://compression.ca/pbzip2). If not
found then it fallbacks to *regular* bzip.


## TODO

Implement a dump command for MySQL/MariaDB/Percona Server using [XtraBackup](http://www.percona.com/software/percona-xtrabackup/).
