<?php
/**
 * @file   sql_secure_dump.drush.inc
 * @author António P. P. Almeida <appa@perusio.net>
 * @date   Tue Nov 16 21:03:54 2010
 *
 * @brief  Implements a drush command for secure database dumps.
 *
 *
 */


// The default secure DB dumps directory.
define('DRUSH_SQL_SECURE_DUMP_DB_DUMP_DIRNAME', '.drush-secure-dumps');
// The default openssl executable.
define('DRUSH_SQL_SECURE_DUMP_OPENSSL_PROGRAM', '/usr/bin/openssl');
// The default password filename.
define('DRUSH_SQL_SECURE_DUMP_PASSWD_FILENAME', '.dump-passwd');
// The recommended password filename mode (in octal).
define('DRUSH_SQL_SECURE_DUMP_PASSWD_FILE_MODE', '0400');


/**
 * Implementation of hook_drush_help().
 */
function sql_secure_dump_drush_help($section) {
  switch ($section) {
    case 'drush:sql-secure-dump':
      return dt('Dumps a database and encrypts it using OpenSSL with DES3');
  }
} // secure_db_dump_drush_help


/**
 * Implementation of hook_drush_command().
 */
function sql_secure_dump_drush_command() {
  $options['--database'] = 'The DB connection key if using multiple connections in settings.php.';
  if (drush_drupal_major_version() >= 7) {
    $options['--target'] = 'The name of a target within the specified database.';
  }
  $items['sql-secure-dump'] = array(
    'callback' => 'drush_sql_secure_dump_execute',
    'description' => 'Exports the Drupal DB as SQL using mysqldump and encrypts the dump.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_CONFIGURATION,
    'examples' => array(
      'drush sql-secure-dump --result-file=../18.sql' => 'Save SQL dump to the directory above Drupal root. Default location is ~.drush-dumps.',
      'drush sql-secure-dump --skip-tables-key=common' => 'Skip standard tables. @see example.drushrc.com',
    ),
    'options' => array(
      'result-file' => 'Save to a file. The file should be relative to Drupal root. If --result-file is provided with no value, then date based filename will be created under ~/drush-backups directory.',
      'skip-tables-key' => 'A key in the $skip_tables array. @see example.drushrc.php. Optional.',
      'structure-tables-key' => 'A key in the $structure_tables array. @see example.drushrc.php. Optional.',
      'tables-key' => 'A key in the $tables array. Optional.',
      'tables-list' => 'A comma-separated list of tables to transfer. Optional.',
      'ordered-dump' => 'Use this option to output ordered INSERT statements in the sql-dump.Useful when backups are managed in a Version Control System. Optional.',
      'create-db' => 'Wipe existing tables.',
      'data-only' => 'Omit CREATE TABLE statements. Postgres only.',
      'ordered-dump' => 'Order by primary key and add line breaks for efficient diff in revision control. Also, faster rsync. Slows down the dump. Mysql only.',
      '--result-file' => 'Save to a file. The file should be relative to Drupal root. If you do not define this value, it will be automatically generated.',
      '--openssl=/path/to/openssl' => 'The location of the openssl executable. If not given it tries to find it in the path.',
      '--passwd-file=/path/to/password_file' => 'The location of the password file to be used when encrypting the dump. The default is ~/' . DRUSH_SQL_SECURE_DUMP_PASSWD_FILENAME . '.',
      '--compression-level=' => 'The bzip algorithm compression level. The default is 1.',
    ) + $options,
    'aliases' => array('sqsec'),
  );
  return $items;
} // sql_secure_dump_drush_command


/**
 * Command callback. Outputs the entire Drupal database in SQL format using
 * mysqldump.
 */
function drush_sql_secure_dump_execute($db_spec = NULL) {
  list($exec, $file) = drush_sql_secure_dump();
  // Avoid the php memory of the $output array in drush_shell_exec().
  if (!$return = drush_op_system($exec)) {
    if ($file) {
      drush_log(dt('Database secure dump saved to !path', array('!path' => $file)), 'success');
    }
  }
  return $return;
} // drush_sql_secure_dump_execute


/**
 * Build a mysqldump/pg_dump/sqlite statement.
 *
 * @param db_spec
 *   For /D6, a $db_url. For D7+, a target in the default DB connection.
 * @return array
 *   An array with items.
 *     1. A mysqldump/pg_dump/sqlite statement that is ready for executing.
 *     2. The filepath where the dump will be saved.
 */
function drush_sql_secure_dump($db_spec = NULL) {
  return drush_sql_build_secure_dump_command(drush_sql_get_table_selection(), $db_spec);
} // drush_sql_secure_dump


/**
 * Build a mysqldump/pg_dump/sqlite statement.
 *
 * @param array $table_selection
 *   Supported keys: 'skip', 'structure', 'tables'.
 * @param db_spec
 *   For D5/D6, a $db_url. For D7, a target in the default DB connection.
 * @return array
 *   An array with items.
 *     1. A mysqldump/pg_dump/sqlite statement that is ready for executing.
 *     2. The filepath where the dump will be saved.
 */
function drush_sql_build_secure_dump_command($table_selection, $db_spec = NULL) {
  $skip_tables = $table_selection['skip'];
  $structure_tables = $table_selection['structure'];
  $tables = $table_selection['tables'];

  $ignores = array();
  $skip_tables  = array_merge($structure_tables, $skip_tables);
  $data_only = drush_get_option('data-only');
  // The ordered-dump option is only supported by MySQL for now.
  // @todo add documention once a hook for drush_get_option_help() is available.
  // @see drush_get_option_help() in drush.inc
  $ordered_dump = drush_get_option('ordered-dump');

  if (is_null($db_spec)) {
    $db_spec = _drush_sql_get_db_spec();
  }
  $database = $db_spec['database'];
  $host = $db_spec['host'];

  // Get the commands.
  $openssl =
    is_executable(DRUSH_SQL_SECURE_DUMP_OPENSSL_PROGRAM) ? DRUSH_SQL_SECURE_DUMP_OPENSSL_PROGRAM : `which openssl`;
  if (empty($openssl)) {
    return drush_set_error('NO_OPENSSL', dt('No openssl executable found.'));
  }

  // Get the output filename,
  if (!($file = drush_get_option('result-file', FALSE))) {
    // No result file defined. We write our own in a specific
    // using our own standard for backup dumps.
    $file = $database . '-' . $host . '_' . date('YMd-His') . '.sql';
    // Creating the directory with the secure db dump.
    $file = drush_create_dir_sql_secure_dump() . '/' . $file;
  }

  switch (_drush_sql_get_scheme($db_spec)) {
    case 'mysql':
      $exec = 'mysqldump';
      if ($file) {
        $exec .= ' --result-file '. $file;
      }
      // mysqldump wants 'databasename' instead of 'database=databasename' for no good reason.
      $extra = ' --single-transaction --opt -Q' . str_replace('--database=', ' ', _drush_sql_get_credentials($db_spec));
      if (isset($data_only)) {
        $extra .= ' --no-create-info';
      }
      if (isset($ordered_dump)) {
        $extra .= ' --skip-extended-insert --order-by-primary';
      }
      $exec .= $extra;

      if (!empty($tables)) {
        $exec .= ' ' . implode(' ', $tables);
      }
      else {
        // Append the ignore-table options.
        foreach ($skip_tables as $table) {
          $ignores[] = "--ignore-table=$database.$table";
        }
        $exec .= ' '. implode(' ', $ignores);

        // Run mysqldump again and append output if we need some structure only tables.
        if (!empty($structure_tables)) {
          $exec .= "; mysqldump --no-data $extra " . implode(' ', $structure_tables);
          if ($file) {
            $exec .= " >> $file";
          }
        }
      }
      break;
    case 'pgsql':
      $create_db = drush_get_option('create-db');
      $exec = 'pg_dump ';
      if ($file) {
        $exec .= ' --file '. $file;
      }
      // Unlike psql, pg_dump does not take a '--dbname=' before the database name.
      $extra = str_replace('--dbname=', ' ', _drush_sql_get_credentials($db_spec));
      if (isset($data_only)) {
        $extra .= ' --data-only';
      }
      $exec .= $extra;
      $exec .= (!isset($create_db) && !isset($data_only) ? ' --clean' : '');

      if (!empty($tables)) {
        foreach ($tables as $table) {
          $exec .= " --table=$table";
        }
      }
      else {
        foreach ($skip_tables as $table) {
          $ignores[] = "--exclude-table=$table";
        }
        $exec .= ' '. implode(' ', $ignores);
        // Run pg_dump again and append output if we need some structure only tables.
        if (!empty($structure_tables)) {
          $schemaonlies = array();
          foreach ($structure_tables as $table) {
            $schemaonlies[] = "--table=$table";
          }
          $exec .= "; pg_dump --schema-only " . implode(' ', $schemaonlies) . $extra;
          if ($file) {
            $exec .= " >> $file";
          }
        }
      }
      break;
    case 'sqlite':
      // Dumping is usually not necessary in SQLite, since all database data
      // is stored in a single file on the filesystem which can be copied just
      // like any other file. But it still has a use in migration purposes and
      // building human-readable diffs and such, so let's do it anyway.
      $exec = _drush_sql_connect();
      // SQLite's dump command doesn't support many of the features of its
      // Postgres or MySQL equivalents. We may be able to fake some in the
      // future, but for now, let's just support simple dumps.
      $exec .= ' ".dump"';
      if ($file = drush_get_option('result-file')) {
        $exec .= ' > '. $file;
      }
      break;
    case 'sqlsrv':
      $exec = "echo 'sqlsrv sql-dump not yet supported'";
      break;
  }

  // Get the compression program to be used.
  $bzip_program = sql_bz_get_bzip_command();
  // Get the compression level.
  $bzip_compression_level = drush_get_option('compression-level');
  if (empty($bzip_compression_level)) {
    // Compress the file. Default compression level.
    $exec .= "; $bzip_program $file";
  }
  else {
    // Compress the file. Specified compression level.
    $exec .= "; $bzip_program -$bzip_compression_level $file";
  }

  // Get the password filename if given or try a default filename if exists.
  $passwd_file = drush_sql_secure_dump_get_pwd_file(drush_get_option('passwd-file'));

  // If the default password file doesn't exist. Then encrypt asking for
  // the password.
  if (!file_exists($passwd_file)) {
    $exec .= "; $openssl des3 -in $file.bz2 -out $file.bz2.enc && rm $file.bz2";
  }
  else { // Use the given password file name.
    $exec .= "; $openssl des3 -in $file.bz2 -out $file.bz2.enc -pass file:$passwd_file && rm $file.bz2";
  }

  return array($exec, "$file.bz2.enc");
} // drush_sql_build_secure_dump_command


/**
 * Get the password filename if exists and check its permissions.
 *
 * @param $passwd_file string
 *   The password filename.
 *
 * @return
 *   Either the password filename or NULL depending if the file exists.
 */
function drush_sql_secure_dump_get_pwd_file($passwd_file = DRUSH_SQL_SECURE_DUMP_PASSWD_FILENANE) {

  if (file_exists($passwd_file)) {
    // Test the file mode. Must be 0400 otherwise signal it and try to fix it.
    if (substr(sprintf("%o", fileperms($passwd_file)), -5) != DRUSH_SQL_SECURE_DUMP_PASSWD_FILE_MODE) {
      drush_log(dt('The password file is not in readonly user only readable mode. Trying to fix it'));
      // Trying to fix the password file permissions.
      if (chmod($passwd_file, DRUSH_SQL_SECURE_DUMP_PASSWD_FILE_MODE)) {
        drush_log(dt('Password file is now in readonly user only readable mode.'));
      }
      else {
        drush_log(dt('Cannot fix the password file mode. Continuing. Please fix it!'));
      }
    }
    return $passwd_file;
  }
  return NULL;
} // drush_sql_secure_dump_get_pwd_file


  /**
   * This function builds the name of the default dump directory. If it doesn't
   * exists it creates the directory.
   *
   *
   * @return
   *   The directory name.
   */
function drush_create_dir_sql_secure_dump() {
  // Get the directory where the dumps are stored by default.
  $dump_dir = $_SERVER['HOME'] . '/' .  DRUSH_SQL_SECURE_DUMP_DB_DUMP_DIRNAME;

  // Check if the directory exists. If not create it.
  if (!is_dir($dump_dir)) {
    drush_mkdir($dump_dir);
  }
  return $dump_dir;
} // drush_create_dir_sql_secure_dump
