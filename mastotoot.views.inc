<?php
/**
 * @file
 * Inform views of the database table structure of Mastodon Toot.
 */

/**
 * Implements hook_views_data().
 */
function mastotoot_views_data() {
  // Implicit relationship to the {node} table.
  $data['mastotoot']['table']['join'] = [
    'node' => [
      'left_field' => 'nid',
      'field' => 'nid',
    ],
  ];
  $data['mastotoot']['table']['group'] = t('Content');
  // Describe our fields.
  $data['mastotoot']['id'] = [
    'title' => t('ID'),
    'help' => t('The primary record ID of this toot.'),
    'field' => [
      'handler' => 'views_handler_field_numeric',
      'click sortable' => TRUE,
    ],
    'filter' => [
      'handler' => 'views_handler_filter_numeric',
    ],
    'sort' => [
      'handler' => 'views_handler_sort',
    ],
    'argument' => [
      'handler' => 'views_handler_argument_numeric',
    ],
  ];
  $data['mastotoot']['nid'] = [
    'title' => t('Nid'),
    'help' => t('The node ID of the node this toot has been created from.'),
    'relationship' => [
      'title' => t('Content'),
      'help' => t('The content this toot has been created from.'),
      'base' => 'node',
      'base field' => 'nid',
      'handler' => 'views_handler_relationship',
      'label' => t('Content'),
    ],
    'filter' => [
      'handler' => 'views_handler_filter_numeric',
    ],
    'argument' => [
      'handler' => 'views_handler_argument_numeric',
    ],
    'field' => [
      'handler' => 'views_handler_field_numeric',
    ],
  ];
  $data['mastotoot']['toot_id'] = [
    'title' => t('Toot ID'),
    'help' => t('The remote ID of this toot.'),
    'field' => [
      'handler' => 'views_handler_field',
      'click sortable' => TRUE,
    ],
    'filter' => [
      'handler' => 'views_handler_filter_string',
    ],
    'sort' => [
      'handler' => 'views_handler_sort',
    ],
    'argument' => [
      'handler' => 'views_handler_argument_string',
    ],
  ];
  $data['mastotoot']['created'] = [
    'title' => t('Share date'),
    'help' => t('The date the toot was shared.'),
    'field' => [
      'handler' => 'views_handler_field_date',
      'click sortable' => TRUE,
    ],
    'sort' => [
      'handler' => 'views_handler_sort_date',
    ],
    'filter' => [
      'handler' => 'views_handler_filter_date',
    ],
  ];
  return $data;
}
