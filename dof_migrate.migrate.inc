<?php
/**
 * @file
 * Base class for the dof_migrate module.
 */

/**
 * Class DofMigrateWordpressWizardExtender.
 *
 * Extender Class for the MigrateUIWizard.
 */
class DofMigrateWordpressWizardExtender extends MigrateUIWizardExtender {

  protected $contentValues = [];
  protected $siteName;

  /**
   * {@inheritdoc}
   */
  public function __construct(DofMigrateWordPressMigrateWizard $wizard, array $wizard_steps) {
    parent::__construct($wizard, $wizard_steps);
    $site_name = variable_get('site_abbreviation', 'dof');
    $arguments_function = $site_name . '_migrate_get_site_arguments';
    $context = $arguments_function();
    $new_step = $this->wizard->addStep(t('Select custom types to import'), [$this, 'customSelectForm'], $wizard_steps[t('Select content to import')], $context['forms']);
    // The extender functionality really doesn't seem finished, so a lot of
    // methods are protected. This is a hacky solution to getallsteps array.
    $step_position = array_search(t('Select content to import'), array_keys($wizard_steps));
    $res = array_merge(array_slice($wizard_steps, 0, $step_position + 1, TRUE),
      ['Select custom types to import' => $new_step],
      array_slice($wizard_steps, $step_position, count($wizard_steps) - 1, TRUE));

    $this->wizard_steps = $res;
    $this->siteName = variable_get('site_abbreviation', 'dof');
  }

  /**
   * Build select form for custom content types.
   *
   * @param array $form_state
   *   Form state array.
   *
   * @return array
   *   Return form array.
   */
  public function customSelectForm(&$form_state) {
    // Pass in arguments to build form.
    $context = $form_state['wizard']->getCurrentStep()->getContext();
    // Get destination node type(s)
    $node_types = node_type_get_types();
    $options = ['' => t('Do not import')];
    foreach ($node_types as $node_type => $info) {
      $options[$node_type] = $info->name;
    }
    foreach (array_values($context) as $details) {
      $content_type = $details['type'];
      $form[$content_type][$content_type . '_type'] = [
        '#type' => 'select',
        '#title' => t('Import WordPress @content_type posts as', ['@content_type' => $content_type]),
        '#options' => $options,
      ];
    }

    return $form;
  }

  /**
   * Validate function for custom step.
   *
   * Adds another step for each content type.
   *
   * @param array $form_state
   *   Form state array.
   */
  public function customSelectFormValidate(&$form_state) {
    $context = $form_state['wizard']->getCurrentStep()->getContext();
    foreach (array_values($context) as $details) {
      $content_type = $details['type'];
      $name = $details['formatted_name'];
      $this->contentValues[$content_type][$content_type . '_type'] = $form_state['values'][$content_type . '_type'];
      if ($this->contentValues[$content_type][$content_type . '_type']) {
        $this->wizard->addStep(t('@content_type', ['@content_type' => $name]), [$this, 'contentCustomForm'], $this->wizard_steps[t('Select custom types to import')], $details);
      }
    }
  }

  /**
   * Custom Type options form.
   *
   * @param array $form_state
   *   Form state array.
   *
   * @return array
   *    Form array.
   */
  public function contentCustomForm(&$form_state) {
    $context = $form_state['wizard']->getCurrentStep()->getContext();
    $bundle = array_values($this->contentValues[$context['type']])[0];
    $fields = ['' => t('Do not import')];
    $field_instances = field_info_instances("node", $bundle);
    foreach ($field_instances as $field_name => $values) {
      $fields[$field_name] = $values['label'];
    }
    $content_type = $context['type'];
    foreach (array_values($context['fields']) as $field) {
      $form[$field] = [
        '#type' => 'select',
        '#title' => t('Import @field field into @content_type content type.', ['@field' => $field, '@content_type' => $content_type]),
        '#options' => $fields,
      ];
    }

    return $form + $this->contentForm($form_state, $content_type);
  }

  /**
   * Media Type validation form.
   *
   * @param array $form_state
   *   Form state array.
   */
  public function contentCustomFormValidate(&$form_state) {
    $context = $form_state['wizard']->getCurrentStep()->getContext();
    $content_type = $context['type'];
    $this->contentValues[$content_type] += $form_state['values'];
    $this->contentValues[$content_type]['formatted_name'] = $context['formatted_name'];
  }

  /**
   * Review form alter.
   *
   * Alter the last stage of the wordpress migrate UI.
   *
   * @param array $form
   *   Form array.
   * @param array $form_state
   *   Form state array.
   */
  public function reviewFormAlter(&$form, &$form_state) {
    foreach ($this->contentValues as $type => $values) {
      if ($this->contentValues[$type][$type . '_type']) {
        $class_name = ucfirst($this->siteName) . 'MigrateWordpress' . $this->contentValues[$type]['formatted_name'] . 'Entry';
        $this->wizard->addMigration($this->contentValues[$type]['formatted_name'], $class_name, $this->contentValues[$type]);
      }
    }
  }

  /**
   * Form for mapping source content (node) types to destination types.
   *
   * @todo: Copied directly from wordpress_migrate,and needs cleaning up.
   */
  public function contentForm(&$form_state, $selected_post_type) {
    $form = [];

    $empty_field = [
      '#type' => 'value',
      '#value' => '',
    ];

    $vocabs = $this->vocabularies($selected_post_type);
    if (!empty($vocabs)) {
      $options = ['' => t('Do not import')];
      foreach ($vocabs as $machine_name => $name) {
        $options[$machine_name] = $name;
      }

      // If field_tags exists, default to it.
      $tags_default = (isset($options['field_tags']) ? 'field_tags' : '');
      $form['tag_field'] = [
        '#type' => 'select',
        '#title' => t('Import WordPress tags to the term reference field'),
        '#default_value' => $tags_default,
        '#options' => $options,
      ];

      $form['category_field'] = [
        '#type' => 'select',
        '#title' => t('Import WordPress categories to the term reference field'),
        '#default_value' => '',
        '#options' => $options,
      ];
    }
    else {
      $form['tag_field'] = $form['category_field'] = $empty_field;
    }

    if (module_exists('comment') &&
      (variable_get('comment_' . $selected_post_type, COMMENT_NODE_OPEN)
        != COMMENT_NODE_CLOSED)) {
      $form['comments'] = [
        '#type' => 'radios',
        '#title' => t('Import comments?'),
        '#options' => [1 => t('Yes'), 0 => t('No')],
        '#default_value' => 1,
      ];
      $form['pingbacks'] = [
        '#type' => 'radios',
        '#title' => t('Ignore pingbacks?'),
        '#options' => [1 => t('Yes'), 0 => t('No')],
        '#default_value' => 1,
      ];
    }
    else {
      $form['comments'] = [
        '#type' => 'value',
        '#value' => 0,
      ];
      $form['pingbacks'] = [
        '#type' => 'value',
        '#value' => 0,
      ];
    }

    $file_fields = ['' => t('Do not import')];
    $file_fields += $this->fileFields($selected_post_type, 'file')
      + $this->fileFields($selected_post_type, 'media');
    $file_image_fields = $file_fields +
      $this->fileFields($selected_post_type, 'image');
    if (count($file_image_fields) > 1) {
      $form['attachment_field'] = [
        '#type' => 'select',
        '#title' => t('Field for attachments (including images)'),
        '#default_value' => '',
        '#options' => $file_image_fields,
        '#states' => [
          'visible' => [
            'input[name="destination_type"]' => ['value' => 'blog'],
          ],
        ],
      ];
    }
    else {
      $form['attachment_field'] = $empty_field;
    }
    if (count($file_fields) > 1) {
      $form['podcast_field'] = [
        '#type' => 'select',
        '#title' => t('Field for Blubrry PowerPress podcasts'),
        '#default_value' => '',
        '#options' => $file_fields,
      ];
    }
    else {
      $form['podcast_field'] = $empty_field;
    }

    $options = [];
    foreach (filter_formats() as $format_id => $format) {
      $options[$format_id] = $format->name;
    }
    $form['text_format'] = [
      '#type' => 'select',
      '#title' => t('Default format for text fields'),
      '#default_value' => 'filtered_html',
      '#options' => $options,
    ];
    $form['text_format_comment'] = [
      '#type' => 'select',
      '#title' => t('Default format for comment text fields'),
      '#default_value' => 'filtered_html',
      '#options' => $options,
      '#states' => [
        'invisible' => [
          'input[name="comments"]' => ['value' => 0],
        ],
      ],
    ];

    if (module_exists('path')) {
      $options = [
        0 => t('Do not set path aliases'),
        1 => t('Set path aliases to their original WordPress values'),
      ];
      $attributes = [];
      $default_value = 1;
      if (module_exists('pathauto')) {
        $options[2] = t('Have pathauto generate new aliases');
        if (!module_exists('migrate_extras')) {
          $form['pathauto_message'] = [
            '#prefix' => '<p>',
            '#markup' => t('<strong>To be able to change the <em>Path alias handling</em>, you must have the <a href="@extras">Migrate Extras module</a> installed and enabled.</strong>',
              ['@extras' => url('http://drupal.org/project/migrate_extras')]),
            '#suffix' => '</p>',
          ];
          $attributes['disabled'] = TRUE;
          $default_value = 2;
        }
      }
      $form['path_action'] = [
        '#type' => 'radios',
        '#title' => t('Path alias handling'),
        '#description' => t('Select how path aliases for imported nodes will be set.'),
        '#options' => $options,
        '#default_value' => $default_value,
        '#attributes' => $attributes,
      ];
    }
    else {
      $form['path_action'] = $empty_field;
    }

    if (module_exists('redirect')) {
      if (class_exists('MigrateRedirectEntityHandler')) {
        $form['generate_redirects'] = [
          '#type' => 'checkbox',
          '#title' => t('Generate redirects'),
          '#description' => t('If this box is checked, redirects will be set up from
            the former WordPress blog URLs to the new URLs on your Drupal site'),
          '#default_value' => FALSE,
        ];
      }
      else {
        $form['generate_redirects'] = [
          '#type' => 'value',
          '#value' => 0,
        ];
        $form['generate_redirects_info'] = [
          '#prefix' => '<p>',
          '#markup' => t('You have the Redirect module enabled. To be able to generate redirects for your imported WordPress content, you need to <a href="@link">patch Redirect</a>.',
            ['@link' => 'http://drupal.org/node/1116408#comment-6040494']),
          '#suffix' => '</p>',
        ];
      }
    }
    else {
      $form['generate_redirects'] = $empty_field;
    }

    return $form;
  }

  /**
   * Return a list of vocabularies attached to a given node type (bundle).
   *
   * @param string $node_type
   *   Node Type.
   *
   * @return array
   *    Term Fields array.
   */
  protected function vocabularies($node_type) {
    $fields = field_info_instances('node', $node_type);
    $term_fields = [];
    foreach ($fields as $field_name => $instance_info) {
      $field_info = field_info_field($field_name);
      if ($field_info['type'] == 'taxonomy_term_reference') {
        $term_fields[$field_name] = $instance_info['label'];
      }
    }
    return $term_fields;
  }

  /**
   * Return a list of file fields attached to a given node type (bundle).
   *
   * @param string $node_type
   *   Node Type.
   * @param string $file_type
   *   File Type.
   *
   * @return array
   *    File Fields array.
   */
  protected function fileFields($node_type, $file_type) {
    $fields = field_info_instances('node', $node_type);
    $file_fields = [];
    foreach ($fields as $field_name => $instance_info) {
      $field_info = field_info_field($field_name);
      if ($field_info['type'] == $file_type) {
        $file_fields[$field_name] = $instance_info['label'];
      }
    }
    return $file_fields;
  }

}

/**
 * Class DofMigrateWordPressMigrateWizard.
 *
 * Helper class to retrieve properties.
 */
class DofMigrateWordPressMigrateWizard extends WordPressMigrateWizard {

  /**
   * {@inheritdoc}
   */
  public function __construct() {
    parent::__construct();
  }

  /**
   * Getter function for the current step.
   */
  public function getCurrentStep() {
    return $this->currentStep;
  }

  /**
   * {@inheritdoc}
   */
  public function getSourceName() {
    return t('WordPress');
  }

}

/**
 * Class DofMigrateWordpressItemMigration.
 *
 * Base helper class for all dof migrations.
 */
class DofMigrateWordpressItemMigration extends WordPressItemMigration {

  /**
   * {@inheritdoc}
   */
  public function __construct(array $arguments = []) {
    parent::__construct($arguments);
  }

  /**
   * {@inheritdoc}
   */
  public function prepareRow($row) {
    // Always include this fragment at the beginning of every prepareRow()
    // implementation, so parent classes can ignore rows.
    if (parent::prepareRow($row) === FALSE) {
      return FALSE;
    }

    // Interpret the wordpress [] short tags.
    $replacements = [
      '[frame_box]' => '<div class="frame_box">',
      '[/frame_box]' => '</div>',
      '[info]' => '<div class="info_box">',
      '[/info]' => '</div>',
      '[line]' => '<div class="line_shortcut" style="padding-top:0px; padding-bottom:0px;"></div>',
      '<h1' => '<h2',
      '</h1>' => '</h2>',
    ];
    $row->content = str_replace(array_keys($replacements), $replacements, $row->content);

    // Attachment import looks for usage of a specific file url taken from the
    // attachment content type, and swaps it out. Instead of trying to alter
    // this behaviour, it is easier to alter the content that is imported to
    // use this specific url.
    // Attachments are brought in with a https uri, but the content isn't
    // consistent. Replace http:// with https:// .
    $row->content = str_replace('http://' . $this->blogDomain(), 'https://' . $this->blogDomain(), $row->content);

    if (!empty($row->content)) {
      $dom = self::domDocumentLoadString($row->content);
      foreach ($dom->getElementsByTagName('a') as $link) {
        if ($new_href = self::linkIsRelative($link->getAttribute('href'), $this->blogDomain())) {
          $link->setAttribute('href', $new_href);
        }
      }

      foreach ($dom->getElementsByTagName('img') as $img) {
        if ($new_src = self::linkIsRelative($img->getAttribute('src'), $this->blogDomain())) {
          $img->setAttribute('src', $new_src);
        }
      }

      $row->content = self::domDocumentSaveString($dom);
    }

    return TRUE;
  }

  /**
   * Helper function to get blog url without protocol.
   */
  public function blogDomain() {
    $disallowed = ['http://', 'https://'];
    foreach ($disallowed as $d) {
      if (strpos($this->blog->getBlogUrl(), $d) === 0) {
        return str_replace($d, '', $this->blog->getBlogUrl());
      }
    }
  }

  /**
   * Convert yes/no fields to boolean.
   */
  public function convertToBoolean($data) {
    return $data == 'yes' ? TRUE : FALSE;
  }

  /**
   * Ignore Image if at particular domain.
   */
  public function ignoreImage($data) {
    if (strpos($data, 'uat.arena.ac3d.rroom.net')) {
      return FALSE;
    }
    return $data;
  }

  /**
   * Remove all whitespace from string.
   */
  public function removeWhitespace($data) {
    return trim(str_replace(' ', '', $data));
  }

  /**
   * Unserialize data.
   *
   * @Todo map multivalue serialised array to link fields.
   */
  public function unserializeLink($data) {
    return FALSE;
  }

  /**
   * Loads markup into a DOMDocument object.
   *
   * Wraps all content in <foo> tags to ensure no orphaned elements in top
   * level. Ensure you export this document to string using
   * self::domDocumentSaveString() rather than DOMDocument->saveXML().
   *
   * @param string $markup
   *   String containing markup of imported article.
   *
   * @return DOMDocument
   *   DOMDocument object.
   *
   * @see self::domDocumentSaveString()
   */
  public static function domDocumentLoadString($markup) {
    $markup = '<foo>' . $markup . '</foo>';
    $xml = new DOMDocument();
    $xml->loadXML($markup);

    return $xml;
  }

  /**
   * Converts DOMDocument object to content string.
   *
   * Use in conjunction with self::domDocumentLoadString() as this removes
   * dummy wrapping element and xml doctype element.
   *
   * Wraps all content in <foo> tags to ensure no orphaned elements in top
   * level. Ensure you export this document to string using
   * self::domDocumentSaveString() rather than DOMDocument->saveXML().
   *
   * @param DOMDocument $xml
   *   DOMDocument to convert to string suitable for HTML markup.
   *
   * @return string
   *   String to use for migration.
   *
   * @see self::domDocumentLoadString()
   */
  public static function domDocumentSaveString(DOMDocument $xml) {
    $replacements = [
      // Dummy XML wrappers.
      '<?xml version="1.0"?>' => '',
      '<foo>' => '',
      '</foo>' => '',

      // HTML Entity replacement.
      '&#160;' => '&nbsp;',
      '&#xA0;' => '&nbsp;',
    ];
    $markup = $xml->saveXML();
    $markup = str_replace(array_keys($replacements), $replacements, $markup);

    return trim($markup);
  }

  /**
   * Crude function to check if a href is a relative link to a file.
   *
   * @param string $url
   *   URL taken from href attribute.
   *
   * @return bool|string
   *   Replacement href or FALSE if not a file, or is absolute.
   */
  public static function linkIsRelative($url, $blog_domain) {
    if (substr($url, 0, 8) == 'https://' || substr($url, 0, 7) == 'http://') {
      return FALSE;
    }
    $new_url = str_replace('../', '/', $url);
    if (substr($new_url, 0, 6) != '/files') {
      return FALSE;
    }

    return 'https://' . $blog_domain . $new_url;
  }

}

/**
 * {@inheritdoc}
 *
 * Class handles custom attachment migration logic.
 */
class DofMigrateWordpressAttachment extends WordPressAttachment {

  /**
   * {@inheritdoc}
   */
  public function __construct(array $arguments = []) {
    parent::__construct($arguments);
  }

  /**
   * {@inheritdoc}
   *
   * Overwrite the postImport method for the Attachment, in order to more
   * generalise the replacements to tags.
   *
   * Method adds 301 redirects from old wordpress attachment paths to new Drupal
   * public files path.
   */
  public function postImport() {
    migrate_instrument_start('WordPressAttachment postImport');
    // We'd like to just go through nodes with attachments (i.e., loop
    // through wordpress_migrate_attachment), but a node may reference
    // other posts' attachments without having any itself.
    foreach ($this->dependencies as $migration_name) {
      $migration = Migration::getInstance($migration_name);
      $map_table = $migration->getMap()->getMapTable();
      $nids = db_select($map_table, 'be')
        ->fields('be', ['destid1'])
        ->isNotNull('destid1')
        ->execute();
      foreach ($nids as $nid_row) {
        $node = node_load($nid_row->destid1);
        if ($node && is_array($node->body)) {
          foreach ($node->body as $language => $body) {
            $body = $body[0]['value'];
            // If we have the media module, rewrite the img tags to media tags
            // if we can.
            if (module_exists('media')) {
              $html = str_get_html($body);
              foreach ($html->find('a') as $element) {
                // Get the full image markup.
                $link = $element->outertext;
                $link_text = $element->innertext;
                $href = $element->href;
                // @todo Remove hard coded value!
                if (strpos($href, $this->blogDomain()) >= 0 && $this->checkFileExtensions($href)) {
                  $file_attachment_map = db_select('wordpress_migrate_attachment', 'wma')
                    ->fields('wma', ['new_fid'])
                    ->condition('wma.original_url', $href, '=')
                    ->execute();
                  foreach ($file_attachment_map->fetchAll() as $row) {
                    if ($row->new_fid) {
                      $file = file_load($row->new_fid);
                      $image_info = [
                        'type' => 'media',
                        'fid' => $file->fid,
                        'view_mode' => 'default',
                        'fields' => [
                          'format' => 'default',
                        ],
                        'attributes' => [
                          'class' => 'file media-element file-default',
                        ],
                        'link_text' => $link_text,
                      ];
                      $media_tag = '[[' . drupal_json_encode($image_info) . ']]';
                      // Replace the image markup up with the media tag.
                      $body = str_replace($link, $media_tag, $body);
                    }
                  }
                }
              }

              $this->referencedFiles = [];
              $body = preg_replace_callback('|<img +(.*?)>|i',
                [$this, 'replaceImgs'], $body);
              // Add any referenced files to the node's attachment field, if
              // one is configured.
              $migration_arguments = $migration->getArguments();
              if (!empty($migration_arguments['attachment_field'])) {
                $attachment_field = $migration_arguments['attachment_field'];
                foreach ($this->referencedFiles as $file) {
                  $node->{$attachment_field}[LANGUAGE_NONE][] = $file;
                }
              }
            }
            // See if any remaining images can be directly replaced.
            $result = db_select('wordpress_migrate_attachment', 'a')
              ->fields('a', ['original_url', 'new_uri', 'new_fid'])
              ->execute();
            foreach ($result as $row) {
              $body = str_replace($row->original_url, $row->new_uri, $body);
            }
            $node->body[$language][0]['value'] = $body;
          }

          // Maintain the original update datestamp.
          $changed = $node->changed;
          $this->disablePathauto($node);
          node_save($node);
          db_update('node')
            ->fields(['changed' => $changed])
            ->condition('nid', $node->nid)
            ->execute();
        }
      }
    }

    // Add redirects from old file attachment URLs to new destination.
    if (module_exists('redirect')) {
      $file_attachment_map = db_select('wordpress_migrate_attachment', 'wma')
        ->fields('wma', [])
        ->condition('wma.blog_id', $this->blog->getBlogID(), '=')
        ->execute();

      foreach ($file_attachment_map->fetchAll() as $row) {
        $old_url = parse_url($row->original_url);
        $source = trim($old_url['path'], '/');
        $file = file_load($row->new_fid);
        $new_url = parse_url(file_create_url($file->uri));

        if ($redirect = redirect_load_by_source($source)) {
          // Redirect entity already exists for this source - we'll just be
          // changing the redirect property.
        }
        else {
          $redirect = new stdClass();
          $redirect->source = $source;
          $redirect->status_code = 301;
          redirect_object_prepare($redirect);
        }

        $redirect->redirect = trim($new_url['path'], '/');
        redirect_save($redirect);
      }
    }
  }

  /**
   * Check for file extension.
   */
  public function checkFileExtensions($link) {
    $extension = ['pdf', 'pptx', 'docx'];
    foreach ($extension as $ext) {
      if (strpos($link, $ext) != 0) {
        return TRUE;
      }
    }
    return FALSE;
  }

  /**
   * Helper function to get blog url without protocol.
   */
  public function blogDomain() {
    $disallowed = ['http://', 'https://'];
    foreach ($disallowed as $d) {
      if (strpos($this->blog->getBlogUrl(), $d) === 0) {
        return str_replace($d, '', $this->blog->getBlogUrl());
      }
    }
  }

}

