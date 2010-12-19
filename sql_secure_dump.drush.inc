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

// The default openssl executable.
define('DEFAULT_OPENSSL_PROGRAM', '/usr/bin/openssl');
// The default DB dumps directory name.
define('DEFAULT_DB_DUMP_DIRNAME', '.drush-dumps');
// The default password filename.
define('DEFAULT_PASSWD_FILENAME', '.dump-passwd');

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
      '--result-file' => 'Save to a file. The file should be relative to Drupal root. If you do not define this value, it will be automatically generated.',
      '--skip-tables-key' => 'A key in the $skip_tables array. @see example.drushrc.php. Optional.',
      '--structure-tables-key' => 'A key in the $structure_tables array. @see example.drushrc.php. Optional.',
      '--openssl=/path/to/openssl' => 'The location of the openssl executable. If not given it tries to find it in the path.',
      '--passwd-file=/path/to/password_file' => 'The location of the password file to be used when encrypting the dump.',
      '--compression-level=' => 'The gzip algorithm compression level. The default is 6.' 
    ) + $options,
    'aliases' => array('sqsec'),
  );
  return $items;
} // sql_secure_dump_drush_command

/**
* Command callback. Outputs the entire Drupal database in SQL format using
* mysqldump.
*/
function drush_sql_secure_dump_execute() {
  static $dumped;
  // Hack to get it to not dump the DB twice.
  // Needs work!
  if (isset($dumped)) {
    return;
  }
  $exec = drush_sql_build_secure_dump_command();
  // Avoid the php memory of the $output array in drush_shell_exec().
  $return = drush_op('system', $exec);
  $dumped = TRUE;
  return $return;
}

/**
* Build a mysqldump statement.
* 
* @param db_spec
*   For D5/D6, a $db_url. For D7, a target in the default DB connection.
* @return string
*   A mysqldump statement that is ready for executing.
*/
function drush_sql_build_secure_dump_command($db_spec = NULL) {
  $skip_tables = $structure_tables = $ignores = array();
 
// Skip large core tables if instructed.  Also used by 'sql load' command.
  $key = drush_get_option('skip-tables-key');
  $all_skip_tables = drush_get_option('skip-tables', array());
  if (array_key_exists($key, $all_skip_tables)) {
    $skip_tables = $all_skip_tables[$key];
  }
 
  // Skip any structure-tables as well.
  $key = drush_get_option('structure-tables-key');
  $all_structure_tables = drush_get_option('structure-tables', array());
  if (array_key_exists($key, $all_structure_tables)) {
    $structure_tables = $all_structure_tables[$key];
    $skip_tables += $structure_tables;
  }
  switch (_drush_sql_get_scheme()) {
    case 'mysql':
      if (is_null($db_spec)) {
        $db_spec = _drush_sql_get_db_spec();
      }
      $database = $db_spec['database'];
      $host = $db_spec['host'];
      
      // Get the commands.
      $openssl = trim(shell_exec('which openssl'));
      // which might not exist so we try brute force.
      if (empty($openssl)) {
        $openssl = DEFAULT_OPENSSL_PROGRAM; 
      }
      // Check if it really exists.
      if (!file_exists($openssl)) {
        return drush_set_error('NO_OPENSSL', dt('No openssl executable found.'));
      }
      
      $exec = 'mysqldump';
      if ($file = drush_get_option('result-file')) {
        $exec .= ' --result-file '. $file;
      }
      else {
          // No result file defined. We write our own in a specific
          // using my own standard for backup dumps
          $file = $database . '-' . $host . '_' . date('YMd-His') . '.sql';
          // Creating the directory with the secure db dump.
          $dump_dir = drush_create_dir_sql_secure_dump();
          $file = $dump_dir . '/' . $file;
          $exec .= ' --result-file ' . $file;
      }
      $extra = ' --single-transaction --opt -Q' . str_replace('--database=', ' ', _drush_sql_get_credentials($db_spec)) ;
      $exec .= $extra;
     
      // Append the ignore-table options.
      foreach ($skip_tables as $table) {
        $ignores[] = "--ignore-table=$database.$table";
      }
      $exec .= ' '. implode(' ', $ignores);
     
      // Run mysqldump again and append output if we need some structure only
      // tables.
      if (!empty($structure_tables)) {
        $exec .= "mysqldump --no-data $extra " . implode(' ', $structure_tables);
        if ($file) {
          $exec .= " >> $file";
        }
      }
      // Get the compression level.
      $gzip_compression_level = drush_get_option('compression-level');
      if (!empty($gzip_compression_level)) {
        // Compress the file. Default compression level. 
        $exec .= "; gzip $file";        
      }
      else {
        // Compress the file. Specified compression level. 
        $exec .= "; gzip -$gzip_compression_level $file";        
      }
      // Get the password file.
      $passwd_file = drush_get_option('passwd-file');
      // If no password file is given ask us the default.
      if (empty($passwd_file)) {
        $passwd_file = $dump_dir . '/' . DEFAULT_PASSWD_FILENAME;
      }
      // If the default password file doesn't exist. Then encrypt asking for
      // the password.
      if (!file_exists($passwd_file)) {
        $exec .= "; $openssl des3 -in $file.gz -out $file.gz.enc && rm $file.gz";
      }
      else { // Use the given password file name.
        $exec .= "; $openssl des3 -in $file.gz -out $file.gz.enc -pass file:$passwd_file && rm $file.gz";
      }
      break;
  }
  return $exec;
} // drush_sql_build_secure_dump_command


/** 
 * This function builds the name of the default dump directory. If it doesn't
 * exists it creates the directory.
 * 
 * 
 * @return 
 */
function drush_create_dir_sql_secure_dump() {
  // Get the directory where the dumps are stored by default.
  $dump_dir = $_SERVER['HOME'] . '/' . DEFAULT_DB_DUMP_DIRNAME; 

  // Check if the directory exists. If not create it.
  if (!is_dir($dump_dir)) {
    drush_mkdir($dump_dir);
  }
  return $dump_dir;
} // drush_create_dir_sql_secure_dump
