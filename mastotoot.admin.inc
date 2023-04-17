<?php
/**
 * @file
 * Admin form and callbacks for Mastodon Toot module.
 */

/**
 * Settings form.
 */
function mastotoot_form($form, &$form_state) {
  $form = [];
  $form['#config'] = 'mastotoot.settings';
  $config = config('mastotoot.settings');

  $form['instance_url'] = [
    '#type' => 'textfield',
    '#title' => 'Instance URL',
    '#default_value' => $config->get('instance_url'),
    '#required' => TRUE,
    '#description' => '@todo',
  ];
  $form['access_token'] = [
    '#type' => 'textfield',
    '#title' => 'Access token',
    '#default_value' => $config->get('access_token'),
    '#required' => TRUE,
    '#description' => '@todo',
  ];

  $content_type = $config->get('content_type');
  if (isset($form_state['values'])) {
    // For Ajax replacements of depending fields.
    $content_type = $form_state['values']['content_type'];
  }
  $form['content_type'] = [
    '#type' => 'select',
    '#title' => 'Node type for toots',
    '#options' => _mastotoot_get_node_type_options(),
    '#default_value' => $content_type,
    '#ajax' => [
      'event' => 'change',
      'callback' => '_mastotoot_ajax_callback',
      'wrapper' => 'mastotoot-replace',
    ],
    '#description' => 'setting to none will disable tooting',
  ];
  $form['fields'] = [
    '#type' => 'container',
    '#prefix' => '<div id="mastotoot-replace">',
    '#suffix' => '</div>',
  ];
  $form['fields']['text_field'] = [
    '#type' => 'select',
    '#title' => 'Field for additional text',
    '#options' => _mastotoot_get_node_field_options('text', $content_type),
    '#default_value' => $config->get('text_field'),
    '#description' => '... additionally to node title ...'
  ];
  // @todo what about entityreference?
  $form['fields']['hashtag_field'] = [
    '#type' => 'select',
    '#title' => 'Field for hashtags',
    '#options' => _mastotoot_get_node_field_options('taxonomy', $content_type),
    '#default_value' => $config->get('hashtag_field'),
    '#description' => '... do not add Rauten ...',
  ];
  $form['#validate'][] = 'mastotoot_form_validate';

  return system_settings_form($form);
}

/**
 * Ajax callback to dynamically replace form parts.
 */
function _mastotoot_ajax_callback($form, $form_state) {
  return $form['fields'];
}

/**
 * Helper function to build select list options from node types.
 *
 * @return array
 */
function _mastotoot_get_node_type_options() {
  $all_types = node_type_get_types();
  $options = ['' => t('- None -')];
  foreach ($all_types as $machine_name => $data) {
    if (!$data->disabled) {
      $options[$machine_name] = $data->name;
    }
  }
  return $options;
}

/**
 * Helper function to provide field options for select lists.
 *
 * @param string $field_type
 * @param string $content_type
 * @return array
 */
function _mastotoot_get_node_field_options($field_type, $content_type) {
  $options = ['' => t('- None -')];
  if (!empty($content_type)) {
    $fields = field_info_instances('node', $content_type);
    foreach ($fields as $name => $definition) {
      if ($definition['widget']['module'] == $field_type) {
        $options[$name] = $definition['label'];
      }
    }
  }
  return $options;
}

/**
 * Validation callback for settings form.
 */
function mastotoot_form_validate($form, &$form_state) {
  // Remove white space.
  $url = trim($form_state['values']['instance_url']);
  if (!valid_url($url, TRUE)) {
    $message = t('Instance URL must be a valid external URL');
    form_set_error('instance_url', $message);
  }
  else {
    // Also remove trailing slash, if any.
    $form_state['values']['instance_url'] = rtrim($url, '/');
  }
}
