<?php
/**
 * @file
 * metadata.features.inc
 */

/**
 * Implements hook_node_info().
 */
function metadata_node_info() {
  $items = array(
    'metadata' => array(
      'name' => t('Repository Item'),
      'base' => 'node_content',
      'description' => t('Holds metadata records harvested from the repositories.'),
      'has_title' => '1',
      'title_label' => t('Title'),
      'help' => '',
    ),
  );
  drupal_alter('node_info', $items);
  return $items;
}
