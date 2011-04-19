<?php
/**
 * @file   sql_7z_dump.drush.inc
 * @author António P. P. Almeida <appa@perusio.net>
 * @date   Sun Dec 19 16:34:45 2010
 * 
 * @brief  Implements a drush command for 7zipped database dumps.
 * p7     
 * 
 */

// The default DB dumps directory name.
define('DEFAULT_DB_DUMP_DIRNAME', '.drush-dumps');
define('DEFAULT_P7ZIP_PROGRAM', '/usr/bin/p7zip');

/**
* Implementation of hook_drush_help().
*/
function sql_7z_dump_drush_help($section) {
  switch ($section) {
    case 'drush:sql-7z-dump':
      return dt('Dumps a database and compresses it using 7zip2');
  }
} // secure_db_dump_drush_help

/**
* Implementation of hook_drush_command().
*/
function sql_7z_dump_drush_command() {
  $options['--database'] = 'The DB connection key if using multiple connections in settings.php.';
  if (drush_drupal_major_version() >= 7) {
    $options['--target'] = 'The name of a target within the specified database.';
  }
  $items['sql-7z-dump'] = array(
    'callback' => 'drush_sql_7z_dump_execute',
    'description' => 'Exports the Drupal DB as SQL using mysqldump and compresses the dump using 7zip2. Default location is ~.drush-dumps.',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_CONFIGURATION,
    'examples' => array(
      'drush sql-7z-dump --result-file=../18.sql' => 'Save SQL dump to the directory above Drupal root.',
      'drush sql-7z-dump --skip-tables-key=common' => 'Skip standard tables. @see example.drushrc.com',
    ),
    'options' => array(
      '--result-file' => 'Save to a file. The file should be relative to Drupal root. If you do not define this value, it will be automatically generated.',
      '--skip-tables-key' => 'A key in the $skip_tables array. @see example.drushrc.php. Optional.',
      '--structure-tables-key' => 'A key in the $structure_tables array. @see example.drushrc.php. Optional.',
      '--compression-level=' => 'The 7zip algorithm compression level. The default is 6.',
    ) + $options,
    'aliases' => array('sq7z'),
  );
  return $items;
} // sql_7z_dump_drush_command

/**
* Command callback. Outputs the entire Drupal database in SQL format using
* mysqldump.
*/
function drush_sql_7z_dump_execute() {
  static $dumped;
  // Hack to get it to not dump the DB twice.
  // Needs work!
  if (isset($dumped)) {
    return;
  }
  $exec = drush_sql_build_7z_dump_command();
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
function drush_sql_build_7z_dump_command($db_spec = NULL) {
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
      
      $exec = 'mysqldump';
      if ($file = drush_get_option('result-file')) {
        $exec .= ' --result-file '. $file;
      }
      else {
          // No result file defined. We write our own in a specific
          // using our own standard for backup dumps.
          $file = $database . '-' . $host . '_' . date('YMd-His') . '.sql';
          // Creating the directory with the secure db dump.
          $dump_dir = drush_create_dir_sql_7z_dump();
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
      
      $p7zip = trim(shell_exec('which p7zip'));
      // which might not exist so we try brute force.
      if (empty($p7zip)) {
        $p7zip = DEFAULT_P7ZIP_PROGRAM; 
      }
      // Check if it really exists.
      if (!file_exists($p7zip)) {
        return drush_set_error('NO_7zIP2', dt('No 7zip2 executable found.'));
      }
      
      // Get the compression level.
      $p7zip_compression_level = drush_get_option('compression-level');
      if (empty($gzip_compression_level)) {
        // Compress the file. Default compression level. 
        $exec .= "; p7zip $file";        
      }
      else {
        // Compress the file. Specified compression level. 
        $exec .= "; p7zip -$7zip_compression_level $file";        
      }        
      break;
  }
  return $exec;
} // drush_sql_build_7z_dump_command


/** 
 * This function builds the name of the default dump directory. If it doesn't
 * exists it creates the directory.
 * 
 * 
 * @return
 *   The directory name.
 */
function drush_create_dir_sql_7z_dump() {
  // Get the directory where the dumps are stored by default.
  $dump_dir = $_SERVER['HOME'] . '/' . DEFAULT_DB_DUMP_DIRNAME; 

  // Check if the directory exists. If not create it.
  if (!is_dir($dump_dir)) {
    drush_mkdir($dump_dir);
  }
  return $dump_dir;
} // drush_create_dir_sql_7z_dump
