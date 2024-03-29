<?php
/**
 * @file
 * Class definition for Mastodon Toot.
 */

/**
 * Handle toots on Mastodon, using their API.
 */
class MastoToot {
  /**
   * @var string
   */
  protected $mastodon_url;

  /**
   * @var array<string, string>
   */
  protected $auth_header;

  /**
   * @var Node
   */
  protected $node;

  /**
   * @var string|null
   */
  protected $toot_id;

  /**
   * @var string
   *
   * @see https://docs.joinmastodon.org/entities/Status/#visibility
   */
  protected $toot_visibility;

  /**
   * @param Node $node
   *   The node entity.
   */
  protected function __construct(Node $node) {
    $config = config('mastotoot.settings');
    $this->mastodon_url = $config->get('instance_url') . '/api/v1/statuses';
    $this->auth_header = [
      'Authorization' => 'Bearer ' . $config->get('access_token'),
    ];
    $this->node = $node;
    if (!$node->isNew()) {
      $this->toot_id = $this->fetchTootId($node->nid);
    }
    $this->toot_visibility = $config->get('visibility');
  }

  /**
   * Construct a new toot.
   *
   * @param Node $node
   *   The node entity.
   *
   * @return MastoToot|false
   *   A MastoToot instance to act on, FALSE in case of problems.
   */
  public static function create(Node $node) {
    $config = config('mastotoot.settings');
    // Catch problems with broken config - unlikely but possible.
    if (empty($config->get('instance_url')) || empty($config->get('access_token'))) {
      watchdog('mastotoot', 'Important settings for Mastodon Toot module are missing.', NULL, WATCHDOG_ERROR);
      return FALSE;
    }
    $toot = new MastoToot($node);
    return $toot;
  }

  /**
   * Whether this toot has been shared on your Mastodon instance.
   *
   * @return bool
   */
  public function isShared() {
    return !empty($this->toot_id) ? TRUE : FALSE;
  }

  /**
   * Create and submit a toot to a Mastodon instance.
   *
   * @return void
   */
  public function submitToot() {
    // @todo consider what to do with very old nodes.
    $node = $this->node;
    if ($node->langcode != LANGUAGE_NONE) {
      $langcode = $node->langcode;
    }
    else {
      $langcode = config_get('system.core', 'language_default');
    }

    $content_data = [
      'status' => $this->buildTootContent(),
      'language' => $langcode,
      'visibility' => $this->toot_visibility,
    ];

    $headers = $this->auth_header;
    $headers['Content-Type'] = 'application/json';
    $options = [
      'headers' => $headers,
      'method' => 'POST',
      'data' => json_encode($content_data),
      'timeout' => 5,
    ];
    $response = backdrop_http_request($this->mastodon_url, $options);
    $instance = config_get('mastotoot.settings', 'instance_url');

    if ($response->code === '200') {
      $data = json_decode($response->data, TRUE);
      // @see https://docs.joinmastodon.org/entities/Status/#id
      if (!empty($data['id'])) {
        $this->addRecord($node->nid, $data['id']);
        backdrop_set_message(t('Successfully posted content to %url.', [
          '%url' => $instance,
        ]));
      }
    }
    else {
      backdrop_set_message(t('Problem when posting content to %url.', [
        '%url' => $instance,
      ]), 'warning');
      $this->logWatchdogError($response);
    }

  }

  /**
   * Delete an existing toot on a Mastodon instance.
   *
   * @return void
   */
  public function deleteToot() {
    $options = [
      'headers' => $this->auth_header,
      'method' => 'DELETE',
      'timeout' => 5,
    ];
    $url = $this->mastodon_url . '/' . $this->toot_id;
    $response = backdrop_http_request($url, $options);

    if ($response->code === '200') {
      $this->deleteRecord($this->node->nid);
      backdrop_set_message(t('Shared post on %instance has been deleted.', [
        '%instance' => config_get('mastotoot.settings', 'instance_url'),
      ]));
    }
    // The remote toot doesn't exist anymore, delete obsolete record.
    elseif ($response->code === '404') {
      $this->deleteRecord($this->node->nid);
      $message = 'Status ID not found remotely, deleted local record.';
      watchdog('mastotoot', $message, NULL, WATCHDOG_WARNING);
    }
    // Other errors, like 401 or 5xx.
    else {
      $this->logWatchdogError($response);
    }
  }

  /**
   * Create toot content from a node.
   *
   * @return string
   *   Toot text content.
   */
  protected function buildTootContent() {
    $config = config('mastotoot.settings')->get();
    $node = $this->node;
    $length = 500;
    $build = [
      'title' => '',
      'text' => '',
      'hashtags' => '',
      'url' => '',
    ];

    $title = strip_tags($node->title);
    $build['title'] = $title;
    $length = $length - backdrop_strlen($title);

    // Provide taxonomy term items as hashtags, if enabled.
    if (!empty($config['hashtag_field'])) {
      $hashtags = $this->termsToHashtags($config['hashtag_field']);
      if (!empty($hashtags)) {
        $build['hashtags'] = $hashtags;
        $length = $length - backdrop_strlen($hashtags);
      }
    }

    $build['url'] = $this->createNodeUrl();
    // URLs always count as 23 characters in Mastodon.
    $length = $length - 23;

    // Add a text from configured field, fill up available characters.
    if (!empty($config['text_field'])) {
      $text_fields = field_get_items('node', $node, $config['text_field']);
      $text_field = reset($text_fields);

      if (!empty($text_field)) {
        $stripped = strip_tags($text_field['value']);
        // Consider newlines.
        $length = $length - (count($build) * 2);
        $text = text_summary($stripped, NULL, $length);
        // Revert newline encoding done by text_summary.
        $build['text'] = html_entity_decode($text);
      }
    }

    $content = array_filter($build);
    return implode("\n\n", $content);
  }

  /**
   * Get the toot ID from database.
   *
   * @param int $nid
   *   The node ID.
   * @return string|null
   *   The toot ID on Mastodon, NULL if not shared yet.
   */
  protected function fetchTootId($nid) {
    $query = db_query('SELECT toot_id FROM {mastotoot} WHERE nid = :nid', [
      ':nid' => $nid,
    ]);
    $result = $query->fetchField();
    return (!empty($result)) ? $result : NULL;
  }

  /**
   * Write record to database.
   *
   * @param int $nid
   *   The node ID.
   * @param string $toot_id
   *   The toot ID gathered from Mastodon.
   *
   * @return void
   */
  protected function addRecord($nid, $toot_id) {
    $query = db_insert('mastotoot')
      ->fields([
        'nid' => $nid,
        'toot_id' => $toot_id,
        'created' => REQUEST_TIME,
      ]
    );
    $query->execute();
  }

  /**
   * @param int $nid
   *   The node ID.
   *
   * @return void
   */
  protected function deleteRecord($nid) {
    db_delete('mastotoot')
      ->condition('nid', $nid)
      ->execute();
  }

  /**
   * Create a full URL to the node.
   *
   * @return string
   */
  protected function createNodeUrl() {
    $config = config('mastotoot.settings')->get();
    $node = $this->node;

    // In hook_node_insert() the alias will not exists yet, unless set manually.
    if (empty($node->path['alias'])) {
      if ($node->path['auto']) {
        if (!function_exists('path_generate_entity_alias')) {
          // @todo Figure out what causes path.inc not being included here.
          require_once BACKDROP_ROOT . '/core/modules/path/path.inc';
        }
        $alias = path_generate_entity_alias($node);
      }
      else {
        // Fallback when automatic alias has been disabled for this node.
        $alias = 'node/' . $node->nid;
      }
    }
    else {
      $alias = $node->path['alias'];
    }

    if (!empty($config['override_base_url'])) {
      $base = $config['override_base_url'];
    }
    else {
      global $base_url;
      $base = $base_url;
    }

    $node_url = $base . '/' . $alias;
    return $node_url;
  }

  /**
   * Convert Backdrop taxonomy terms to hashtags.
   *
   * @param string $field_name
   *   Machine name of the field to get terms from.
   *
   * @return string
   */
  protected function termsToHashtags($field_name) {
    $hashtags = '';
    $field_items = field_get_items('node', $this->node, $field_name);
    if ($field_items) {
      $tags = [];
      foreach ($field_items as $index => $item) {
        // Scheduled nodes only provide the "tid", when published.
        $term = taxonomy_term_load($item['tid']);
        if ($term) {
          $tags[] = '#' . $this->camelCase($term->name);
        }
      }
      $hashtags = implode(' ', $tags);
    }
    return $hashtags;
  }

  /**
   * Replaces whitespace and makes the string camel case.
   *
   * @param string $name
   *   The taxonomy term name.
   * @return string
   *   The converted term, usable as hashtag.
   */
  protected function camelCase($name) {
    if (preg_match('/\s/', $name)) {
      $words = explode(' ', $name);
      $result = [];
      // @todo filter out chars not appropriate for hashtags, but allowed in
      // terms?
      foreach ($words as $index => $word) {
        if ($index == 0) {
          $result[] = $word;
        }
        else {
          $result[] = ucfirst($word);
        }
      }
      $name = implode('', $result);
    }
    return $name;
  }

  /**
   * @param object $response
   *   Response as provided by backdrop_http_request().
   *
   * @return void
   */
  protected function logWatchdogError($response) {
    $message = t('Got error code @code.', [
      '@code' => $response->code,
    ]);

    if (!empty($response->status_message)) {
      $message .= ' ' . $response->status_message;
    }
    if (!empty($response->data)) {
      $message .= ': ' . $response->data;
    }
    if (isset($response->error)) {
      $message .= ' ' . $response->error;
    }

    watchdog('mastotoot', $message, NULL, WATCHDOG_ERROR);
  }

}
