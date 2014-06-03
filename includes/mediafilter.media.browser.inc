<?php
/**
 * @file
 * Contains callback function for media file list.
 */

/**
 * AJAX Callback function to return a list of media files.
 */
function mediafilter_media_browser_list() {
  module_load_include('inc', 'media', '/includes/media.browser');

  $params = drupal_get_query_parameters();
  array_walk_recursive($params, '_media_recursive_check_plain');

  $remote_types = !empty($params['types']) ? $params['types'] : NULL;
  $url_include_patterns = !empty($params['url_include_patterns']) ? $params['url_include_patterns'] : NULL;
  $url_exclude_patterns = !empty($params['url_exclude_patterns']) ? $params['url_exclude_patterns'] : NULL;
  $allowed_schemes = !empty($params['schemes']) ? array_filter($params['schemes']) : array();

  $start = isset($params['start']) ? $params['start'] : 0;
  $limit = isset($params['limit']) ? $params['limit'] : media_variable_get('browser_pager_limit');

  $query = db_select('file_managed', 'f');
  $query->fields('f', array('fid'));
  $query->range($start, $limit);
  $query->orderBy('f.timestamp', 'DESC');

  // Add conditions based on remote file type *or* local allowed extensions.
  $or_condition = db_or();

  // Include local files with the allowed extensions.
  if (!empty($params['file_extensions'])) {
    $extensions = array_filter(explode(' ', $params['file_extensions']));
    $local_wrappers = array_intersect_key(media_get_local_stream_wrappers(), $allowed_schemes);
    if (!empty($local_wrappers) && !empty($extensions)) {
      $local_condition = db_or();
      foreach (array_keys($local_wrappers) as $scheme) {
        foreach ($extensions as $extension) {
          $local_condition->condition('f.uri', db_like($scheme . '://') . '%' . db_like('.' . $extension), 'LIKE');
        }
      }
      $or_condition->condition($local_condition);
    }
  }

  // Include remote files with the allowed file types.
  if (!empty($remote_types)) {
    $remote_wrappers = array_intersect_key(media_get_remote_stream_wrappers(), $allowed_schemes);
    if (!empty($remote_wrappers)) {
      $remote_condition = db_and();
      $wrapper_condition = db_or();
      foreach (array_keys($remote_wrappers) as $scheme) {
        $wrapper_condition->condition('f.uri', db_like($scheme . '://') . '%', 'LIKE');
      }
      $remote_condition->condition($wrapper_condition);
      $remote_condition->condition('f.type', $remote_types, 'IN');
      $or_condition->condition($remote_condition);
    }
  }

  if ($or_condition->count()) {
    $query->condition($or_condition);
  }

  if ($url_include_patterns) {
    $query->condition('f.uri', '%' . db_like($url_include_patterns) . '%', 'LIKE');
    // Insert stream related restrictions here.
  }
  if ($url_exclude_patterns) {
    $query->condition('f.uri', '%' . db_like($url_exclude_patterns) . '%', 'NOT LIKE');
  }

  // @todo Implement granular editorial access: http://drupal.org/node/696970.
  //   In the meantime, protect information about private files from being
  //   discovered by unprivileged users. See also media_view_page().
  if (!user_access('administer media')) {
    $query->condition('f.uri', db_like('private://') . '%', 'NOT LIKE');
  }

  $query->condition('f.status', FILE_STATUS_PERMANENT);

  foreach (array_keys(media_get_hidden_stream_wrappers()) as $name) {
    $query->condition('f.uri', db_like($name . '://') . '%', 'NOT LIKE');
  }

  // Set our custom filter option.
  if (isset($params['selection']['users']) && is_array($params['selection']['users'])) {
    $query->condition('f.uid', $params['selection']['users'], 'IN');
  }

  // Set our custom filter option.
  if (isset($params['selection_filename'])) {
    $query->condition('f.filename', '%' . db_like($params['selection_filename']) . '%', 'LIKE');
  }

  $fids = $query->execute()->fetchCol();
  $files = file_load_multiple($fids);
  foreach ($files as $file) {
    media_browser_build_media_item($file);
  }

  drupal_json_output(array('media' => array_values($files)));
  exit();
}
