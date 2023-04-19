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
    '#title' => t('Instance URL'),
    '#default_value' => $config->get('instance_url'),
    '#required' => TRUE,
    '#description' => t('URL of the Mastodon instance to post to, including "https", no trailing slash. For example !example.', [
      '!example' => '<code>https://myinstance.social</code>',
    ]),
  ];
  $form['access_token'] = [
    '#type' => 'textfield',
    '#title' => t('Access token'),
    '#default_value' => $config->get('access_token'),
    '#required' => TRUE,
    '#description' => t('The access token created on your instance for your Mastodon account.'),
  ];

  $content_type = $config->get('content_type');
  if (isset($form_state['values'])) {
    // For Ajax replacements of depending fields.
    $content_type = $form_state['values']['content_type'];
  }
  $form['content_type'] = [
    '#type' => 'select',
    '#title' => t('Content type for toots'),
    '#options' => _mastotoot_get_node_type_options(),
    '#default_value' => $content_type,
    '#ajax' => [
      'event' => 'change',
      'callback' => '_mastotoot_ajax_callback',
      'wrapper' => 'mastotoot-replace',
    ],
    '#description' => t('The content type to share on Mastodon. Setting to none will disable tooting.'),
  ];
  $form['fields'] = [
    '#type' => 'container',
    '#prefix' => '<div id="mastotoot-replace">',
    '#suffix' => '</div>',
  ];
  $form['fields']['text_field'] = [
    '#type' => 'select',
    '#title' => t('Field for additional text'),
    '#options' => _mastotoot_get_node_field_options('text', $content_type),
    '#default_value' => $config->get('text_field'),
    '#description' => t('By default the toot content will be the node title. Choose a text field for more content.'),
  ];
  // @todo Consider entityreference fields?
  $form['fields']['hashtag_field'] = [
    '#type' => 'select',
    '#title' => t('Field for hashtags'),
    '#options' => _mastotoot_get_node_field_options('taxonomy', $content_type),
    '#default_value' => $config->get('hashtag_field'),
    '#description' => t('Backdrop terms will get converted to hashtags and added to toot content.'),
  ];
  $form['#validate'][] = 'mastotoot_form_validate';

  return system_settings_form($form);
}

/**
 * Ajax callback to dynamically replace form parts.
 *
 * @return array<string, array>
 */
function _mastotoot_ajax_callback($form, $form_state) {
  return $form['fields'];
}

/**
 * Helper function to build select list options from node types.
 *
 * @return array<string, string>
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
 *   Machine name of the field.
 * @param string $content_type
 *   Machine name of the node type.
 * @return array<int|string, string>
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
  // Trim unintentionally copy-pasted whitespace from token.
  $form_state['values']['access_token'] = trim($form_state['values']['access_token']);
}

/**
 * Small form to verify Mastodon token auth.
 */
function mastotoot_verify_token_form($form, &$form_state) {
  $config = config('mastotoot.settings')->get();
  $form['help'] = [
    '#type' => 'help',
    '#markup' => t('Verify that your configured URL and token work.'),
  ];
  $show_submit = TRUE;
  if (empty($config['instance_url']) || empty($config['access_token'])) {
    $info = t('No setting for instance URL or access token.');
    $show_submit = FALSE;
  }
  else {
    $info = '<dl class="mastotoot-settings">';
    $info .= '<dt>' . t('Your instance URL') . '</dt>';
    $info .= '<dd>' . $config['instance_url'] . '</dd>';
    $info .= '<dt>' . t('Your access token') . '</dt>';
    $info .= '<dd>' . $config['access_token'] . '</dd>';
    $info .= '</dl>';
  }
  $form['info'] = [
    '#type' => 'markup',
    '#markup' => $info,
  ];
  if ($show_submit) {
    $form['submit'] = [
      '#type' => 'submit',
      '#value' => t('Verify'),
      '#submit' => ['mastotoot_verify_submit'],
    ];
  }
  return $form;
}

/**
 * Submission callback for access token verification.
 *
 * Note: this only checks, if the token is accepted, not if it works for
 * posting.
 *
 * @return void
 */
function mastotoot_verify_submit($form, &$form_state) {
  $config = config('mastotoot.settings')->get();
  // We can not use '/api/v1/apps/verify_credentials' here, as that only checks
  // for scope "read" (full), which is more than this app needs.
  // Scopes required for our app:
  // - read:statuses
  // - write:statues
  // - read:accounts (for this check only)
  $verify_url = $config['instance_url'] . '/api/v1/accounts/verify_credentials';
  $options = [
    'headers' => [
      'Authorization' => 'Bearer ' . $config['access_token'],
    ],
    'method' => 'GET',
    'timeout' => 5,
  ];
  $response = backdrop_http_request($verify_url, $options);

  switch ($response->code) {
    case '200':
      backdrop_set_message(t('Success: your access token is accepted at %url.', [
        '%url' => $config['instance_url'],
      ]));
    break;

    case '401':
    case '403':
      $message = '<strong>' . $response->status_message . '</strong> ';
      $info = json_decode($response->data, TRUE);
      foreach ($info as $key => $value) {
        $message .= $key . ': ' . $value;
      }
      backdrop_set_message($message, 'warning');
    break;

    case '404':
      backdrop_set_message(t('Got error: %url not found.', [
        '%url' => $verify_url,
      ]), 'warning');
    break;

    default:
      backdrop_set_message(t('Unknown problem, see full debug output below.'), 'error');
      // Not sure what other output could be helpful.
      debug($response);
  }
}
