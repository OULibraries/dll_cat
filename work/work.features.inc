<?php
/**
 * @file
 * work.features.inc
 */

/**
 * Implements hook_node_info().
 */
function work_node_info() {
  $items = array(
    'dll_work' => array(
      'name' => t('DLL Work'),
      'base' => 'node_content',
      'description' => t('Holds DLL works'),
      'has_title' => '1',
      'title_label' => t('Title'),
      'help' => '',
    ),
  );
  drupal_alter('node_info', $items);
  return $items;
}
