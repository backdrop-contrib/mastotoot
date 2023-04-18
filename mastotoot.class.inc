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
   * @var string
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
  //protected $toot_visibility = 'public';
  // @todo reset to public. Create UI?
  protected $toot_visibility = 'unlisted';

  /**
   * @param Node $node
   */
  public function __construct(Node $node) {
    $config = config('mastotoot.settings');
    if (empty($config->get('content_type')) || empty($config->get('instance_url'))) {
      // Nothing to do.
      return;
    }

    $this->mastodon_url = $config->get('instance_url') . '/api/v1/statuses';
    $this->auth_header = [
      'Authorization' => 'Bearer ' . $config->get('access_token'),
    ];
    $this->node = $node;
    if (!$node->isNew()) {
      $this->toot_id = $this->fetchTootId($node->nid);
    }
  }

  /**
   * Create and submit a toot to a Mastodon instance.
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

    if ($response->code === '200') {
      $data = json_decode($response->data, TRUE);
      // @see https://docs.joinmastodon.org/entities/Status/#id
      if (!empty($data['id'])) {
        $this->addRecord($node->nid, $data['id']);
        backdrop_set_message('it worked!');
      }
    }
    else {
      // 404, 401, ... 5xx
      backdrop_set_message('0oooops!', 'warning');
      // @todo watchdog, wording...
    }

  }

  /**
   * Delete an existing toot on a Mastodon instance.
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
      backdrop_set_message('deleted!');
    }
    else {
      backdrop_set_message('0oooops!', 'warning');
      // @todo message and watchdog on failure.
    }
  }

  /**
   * Create toot content from a node.
   *
   * @return string
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
    $length = $length - mb_strlen($title);

    // Provide taxonomy term items as hashtags.
    if (!empty($config['hashtag_field'])) {
      $tags_field = field_get_items('node', $node, $config['hashtag_field']);
      if ($tags_field) {
        $tags = [];
        foreach ($tags_field as $index => $item) {
          // Scheduled nodes only provide the "tid", when published.
          $term = taxonomy_term_load($item['tid']);
          $tags[] = '#' . $this->camelCase($term->name);
        }
        $hashtags = implode(' ', $tags);
        $build['hashtags'] = $hashtags;
        $length = $length - mb_strlen($hashtags);
      }
    }

    // No way to get the alias in hook_node_insert, unless set manually.
    // @todo also check "auto" path setting.
    if (empty($node->path['alias'])) {
      if (!function_exists('path_generate_entity_alias')) {
        // What the heck is going on here??
        require_once BACKDROP_ROOT . '/core/modules/path/path.inc';
      }
      $alias = path_generate_entity_alias($node);
    }
    else {
      // Either set manually, or we're not in hook_node_insert().
      $alias = $node->path['alias'];
    }
    global $base_url;
    $build['url'] = $base_url . '/' . $alias;
    // URLs have a fixed charcount in Mastodon. @todo verify
    $length = $length - 23;

    // Add a text from configured field, fill up available characters.
    if (!empty($config['text_field'])) {
      $text_fields = field_get_items('node', $node, $config['text_field']);
      $text_field = reset($text_fields);

      if (!empty($text_field)) {
        $stripped = strip_tags($text_field['value']);
        // Consider newlines.
        $length = $length - (count($build) * 2);
        $build['text'] = text_summary($stripped, NULL, $length);
      }
    }

    $content = array_filter($build);
    return implode("\n\n", $content);
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
   * @param int $nid
   * @return string|null
   */
  protected function fetchTootId($nid) {
    $query = db_query('SELECT toot_id FROM {mastotoot} WHERE nid = :nid', [
      ':nid' => $nid,
    ]);
    $result = $query->fetchField();
    return (!empty($result)) ? $result : NULL;
  }

  /**
   * @param int $nid
   * @param string $toot_id
   */
  protected function addRecord($nid, $toot_id) {
    $query = db_insert('mastotoot')
      ->fields([
        'nid' => $nid,
        'toot_id' => $toot_id,
      ]
    );
    $query->execute();
  }

  /**
   * @param int $nid
   */
  protected function deleteRecord($nid) {
    db_delete('mastotoot')
      ->condition('nid', $nid)
      ->execute();
  }

  /**
   * Replaces whitespace and makes the string camel case.
   *
   * @param string $name
   * @return string
   */
  protected function camelCase($name) {
    if (preg_match('/\s/', $name)) {
      $words = explode(' ', $name);
      $result = [];
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

}
