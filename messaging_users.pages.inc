<?php
/**
 * @file
 * Pages for messaging friend module
 * 
 * We use this indirection so tabs are showed always when reading the messages
 */
function messaging_users_page($account, $type, $mqid = NULL) {
  drupal_add_css(drupal_get_path('module', 'messaging_users') .'/messaging_users.css');
  
  if ($mqid && ($message = messaging_users_load($mqid)) && messaging_users_access('read', $account, $message)) {
    return messaging_users_page_read($account, $message, $type);
  }
  elseif ($type == 'inbox') {
    return messaging_users_page_inbox($account);
  }
  elseif ($type == 'outbox') {
    return messaging_users_page_outbox($account);
  }
}

/**
 * Inbox reader page
 */
function messaging_users_page_inbox($account) {
  
  $output = '';
  // Get messages for this user
  $messages = array();
  
  $header = array(
    array(),
    array('data' => t('From'), 'field' => 'u.name'),
    array('data' => t('Subject'), 'field' => 'm.subject'),
    array('data' => t('Date'), 'field' => 'm.sent', 'sort' => 'desc'),
    //array('data' => t('Status'), 'field' => 'p.status'),
  );
  
  // Fetch messages not deleted
  $sql = 'SELECT p.mqid, p.status, m.subject, m.sent, m.sender AS uid, u.name FROM {messaging_users} p INNER JOIN {messaging_store} m ON m.mqid = p.mqid INNER JOIN {users} u ON p.sender = u.uid WHERE p.receiver = %d AND p.status <> %d';
  $sql .= tablesort_sql($header);
  
  $result = pager_query($sql, 50, 0, NULL, $account->uid, MESSAGING_USERS_STATUS_DELETE);

  while ($msg = db_fetch_object($result)) {
    $messages[] = $msg;
  }
  if ($messages) {
    $operations = array(
      'read' => t('Mark as read'),
      'delete' => t('Delete'),
    );
    $output .= drupal_get_form('messaging_users_reader', $account, 'inbox', $messages, $header, $operations);
  } else {
    $output .= t('You have received no messages yet');
  }
  return $output;
}

/**
 * Outbox reader page
 */
function messaging_users_page_outbox($account) {
  $output = '';
  // Get messages for this user
  $messages = array();
  
  $header = array(
    array(),
    t('To'),
    array('data' => t('Subject'), 'field' => 'm.subject'),
    array('data' => t('Date'), 'field' => 'm.sent', 'sort' => 'desc'),
  ); 
  // We use the queue column to mark deleted messages
  $sql = "SELECT m.mqid FROM {messaging_store} m WHERE m.sender = %d AND m.method = 'users' AND m.queue = 0";
  $sql .= tablesort_sql($header);
  
  $result = pager_query($sql, 50, 0, NULL, $account->uid);

  while ($out = db_fetch_object($result)) {
    // We do a full message loading so we can get all destinations
    $messages[] = messaging_message_load($out->mqid);
  }
  if ($messages) {
    $operations = array(
      'delete' => t('Delete'),
    );
    $output .= drupal_get_form('messaging_users_reader', $account, 'outbox', $messages, $header, $operations);
  } else {
    $output .= t('You have not sent any messages');
  }
  return $output;  
}

/**
 * Read a single message, full page
 */
function messaging_users_page_read($account, $message, $type = NULL) {
  global $user;

  $sender = $message->sender_account;
  
  // We show different fieds depending on whether it is inbox or outbox
  if ($type == 'inbox') {
    if ($message->sender_account->uid) {
      $picture_path = $sender->picture;
      if (empty($picture_path) || !file_exists($picture_path)) {
        $picture_path = variable_get('user_picture_default', '');
      }
      $form['from'] = array(
        '#type' => 'item',
        '#title' => t('From'),
      );
      $form['from']['picture'] = array(
        '#value' => $picture_path,
        '#weight' => 0,
        '#theme' => 'messaging_users_picture',
        '#imagecache_preset' => 'user_medium',
        '#account' => $message->sender_account,
      );
      $form['reply'] = array(
        '#title' => 'reply',
        '#type' => 'hidden',
        '#value' => 'user/' . $GLOBALS['user']->uid . '/messages/reply/' . $message->mqid,
        '#theme' => 'messaging_users_page_read_links',
        '#weight' => 40,
      );
    }
    $form['delete'] = array(
      '#title' => 'delete',
      '#type' => 'hidden',
      '#value' => 'user/' . $GLOBALS['user']->uid . '/messages/delete/' . $message->mqid,
      '#theme' => 'messaging_users_page_read_links',
      '#weight' => 41,
    );
    // If ignore flags enabled, show Ignore flag link
    if (!user_access('send messages to all users', $sender) && ($flag = _messaging_users_flag_get('ignore'))) {
      if ($flag->user_access() && $flag->applies_to_content_object($sender)) {
        $form['from']['ignore'] = array(
          // @todo Make this better
          '#type' => 'item',
          //'#title' => $flag->get_title($sender->uid),
          '#value' => $flag->theme($flag->is_flagged($sender->uid) ? 'unflag' : 'flag', $sender->uid),
          '#attributes' => array('class' => 'flag-profile-' . $flag->name),
        );
      }    
    }
    // Mark it as read if not already read
    if ($message->status == MESSAGING_USERS_STATUS_UNREAD) {
      db_query('UPDATE {messaging_users} SET status = %d WHERE mqid = %d AND receiver = %d', MESSAGING_USERS_STATUS_READ, $message->mqid, $user->uid);
    }
  }
  elseif ($type == 'outbox') {
    $form['to'] = array(
      '#type' => 'item',
      '#title' => t('To'),
      '#weight' => 10,
      '#imagecache_preset' => 'user_medium',
    );
    if (count($message->destinations) > 1) {
      $form['to']['#value'] = messaging_users_format_receivers($message->destinations, 'list');
    }
    else {
      $picture_path = user_load($message->destinations[0])->picture;
      if (empty($picture_path) || !file_exists($picture_path)) {
        $picture_path = variable_get('user_picture_default', '');
      }
      $form['to']['#value'] = $picture_path;
      $form['to']['#theme'] = 'messaging_users_picture';
      $form['to']['#account'] = user_load($message->destinations[0]);
    }
  }
  $form['subject'] = array(
    '#prefix' => '<div class="container-inline">',
    '#type' => 'item',
    '#title' => t('Subject'),
    '#value' => check_markup($message->subject),
    '#weight' => 20,
    '#suffix' => '</div>',
  );
  // @todo Apply input filters here
  $form['content'] = array(
    '#type' => 'item',
    '#title' => t('Message'),
    '#value' => '<span>'. check_markup($message->body, messaging_method_info('users', 'filter'), FALSE) .'</span>',
    '#weight' => 30,
  );
  $form['#theme'] = 'messaging_users_page_read';
  return drupal_render($form);
}

/**
 * Get and format message destinations
 * 
 * @param $type
 *   list, line (compressed)
 */
function messaging_users_format_receivers($destinations, $type = 'line', $limit = 0) {
  if ($destinations) {
    $count = count($destinations);
    if ($limit && $count > $limit) {
      $destinations = array_slice($destinations, 0, $limit);
    }
    // If there is only 1 recipient then render the user image
    if ($count === 1) {
      return messaging_users_format_destination($destinations[0], TRUE);
    }
    if ($type == 'line') {
      $destinations = array_map('messaging_users_format_destination', $destinations);
      return implode(', ', $destinations) . $more;
    } elseif ($type == 'list') {
      return theme('messaging_users_destinations_list', $destinations);
    }
  }
}

/**
 * Format message destination
 */
function messaging_users_format_destination($to, $img = FALSE) {
 $type = $img ? 'user_picture' : 'username';
 if (is_numeric($to)) {
    return theme($type, messaging_load_user($to));
  } elseif (is_object($to)) {
    return theme($type, $to);
  } elseif (is_string($to) && ($special = _messaging_users_special_destinations('key', $to))) {
    return $special;
  } else {
    return $to;
  }
}

/**
 * Reply page callback
 *
 * @param stdClass $account
 *  the user account replying to a message
 *
 * @param stdClass $message
 *  the message object the $account is replying too
 *
 * @return string
 *  the html form
 */
function messaging_users_reply($account, $message) {
  return drupal_get_form('messaging_users_compose', $account, array($message->sender), t('Re: @title', array('@title' => $message->subject)));
}

/**
 * Message composition page
 */
function messaging_users_page_compose($account, $op = NULL, $param = NULL) {
  $destinations = array();
  
  if ($op == 'uid' && is_numeric($param)) {
    $destinations[] = $param;
  }
  $output .= drupal_get_form('messaging_users_compose', $account, $destinations);
  return $output;
}

/**
 * Compose form
 */
function messaging_users_compose(&$form_state, $account, $destinations = array(), $title = '') {

  $form['sender'] = array('#type' => 'value', '#value' => $account);

  // Help notes about destinations
  if (user_access('send messages to all users', $account)) {
    $description = t('Write user names separated by commas. You can also use special group destinations like <i>All users</i> or <i>Role xxx</i>.');
  } elseif (user_access('send messages to users', $account)) {
    $description = t('You can send messages to all active users. Write one or more user names separated by commas.');
  } else {
    $description = t('You can send messages to your friends. Write one or more user names separated by commas.');
  }
  $form['to'] = array(
    '#title' => t('To:'),
    '#type' => 'fieldset',
    '#weight' => -10,
  );
  $form['to']['list'] = array(
    '#prefix' => '<div id="messaging-destination-list" >',
    '#value' => messaging_users_destinations_subform($destinations),
    '#suffix' => '</div>',
  );

  // This will be passed the first time on $form_state['values']
  $form['to']['list']['destinations'] = array('#type' => 'hidden', '#value' => _messaging_users_destinations2string($destinations));

  $ahah_add =   array(
    'path' => 'messaging_users/js/destinations',
    'wrapper' => 'messaging-destination-list',
    'type' => 'replace',    
  ); 
  $form['to']['sendto'] = array(
    '#type' => 'textfield', 
    '#autocomplete_path' => 'messaging_users/js/autocomplete',
    '#ahah' => $ahah_add,
    '#description' => $description,
    '#access' => messaging_users_access('compose'),
  );  
  /*$form['to']['add'] = array(
    '#type' => 'button',
    '#value' => t('Add'),
    '#ahah' => $ahah_add,
  );
  $form['to']['clear'] = array(
    '#type' => 'button',
    '#value' => t('Clear'),
    '#ahah' => array('path' => 'messaging_users/js/destinations/clear') + $ahah_add,
  );*/

  $form['message'] = array(
    '#type' => 'fieldset', 
    '#tree' => TRUE,
    '#weight' => 10,
  );
  $form['message']['subject'] = array(
    '#type' => 'textfield',
    '#prefix' => '<div class="container-inline">',
    '#title' => t('Subject'),
    '#required' => TRUE,
    '#default_value' => $title,
    '#suffix' => '</div>',
  );
  $form['message']['body'] = array(
    '#type' => 'textarea',
    '#title' => t('Message'),
    '#cols' => 55,
    '#default_value' => !empty($GLOBALS['user']->signature) ? "<p>&nbsp;</p><p>---<br/>" . $GLOBALS['user']->signature . '</p>': '',
  );
  // Add in the filter form and then hide it away
  // This is for WYSIWYG
  $form['message']['format'] = filter_form();
  $form['message']['format']['#prefix'] = '<div style="display:none">';
  $form['message']['format']['#suffix'] = '</div>';
  $form['buttons']['#weight'] = 20;
  $form['buttons']['send'] = array('#type' => 'submit', '#value' => t('Send'));
  $form['buttons']['preview'] = array(
    '#type' => 'button', 
    '#value' => t('Preview'),
    //'#submit' => array('messaging_users_compose_preview'),
  );

  // Add preview and destination list
  $form['#after_build'] = array('messaging_users_compose_after_build');
  return $form;
}

/**
 * Form after build hook.
 * - Add destinations here so they are refreshed each time
 */
function messaging_users_compose_after_build(&$form, &$form_state) {
  $op = isset($form_state['values']['op']) ? $form_state['values']['op'] : '';
  
  // The first time it comes from the form itself. Next times from form submission
  if ($op) {
    $destinations = _messaging_users_get_destinations();
    //$destinations = !empty($form_state['values']['receivers']) ? $form_state['values']['receivers'] : array();
    $form['to']['list']['destinations']['#value'] = _messaging_users_destinations2string($destinations);
    $form['to']['list']['#value'] = messaging_users_destinations_subform($destinations);
    $form['to']['sendto']['#value'] = '';
  }
  if ($op == t('Preview') && ($message = messaging_users_message_prepare($form_state['values']['message']))) {
    $form['message_preview'] = array(
      '#type' => 'fieldset',
      '#title' => t('Preview'),
    );
    $form['message_preview']['text'] = array('#value' => theme('messaging_users_message_preview', $message));
  }
  return $form;
}

/**
 * Apply filters before printing out the message
 */
function messaging_users_message_prepare($message) {
  $prepare = array('subject' => '', 'body' => '');
  if (!empty($message['subject'])) {
    $prepare['subject'] = check_plain($message['subject']);
  }
  if (!empty($message['body'])) {
    // we save the body as is and then check_markup on render
    $prepare['body'] = $message['body'];
  }
  return $prepare;
}

/**
 * Form validate
 */
function messaging_users_compose_validate($form, &$form_state) {

  // Get destinations from hidden field and sendto field
  $destinations = _messaging_users_get_destinations();
  
  if ($errors = _messaging_users_validate_destinations($form_state['values']['sender'], $destinations)) {
    foreach ($errors as $msg) {
      form_set_error('sendto', $msg);
    }
  }
  // If any left after validation and filtering...
  if ($destinations) {
    $form_state['values']['receivers'] = $destinations;
  } else {
    form_set_error('sendto', t('Please select one or more recipients for your message.'));
  }
  
}

/**
 * Get destinations from form store and from posts.
 * 
 * Add some additional checkings because this may be coming from an ajax request too.
 */
function _messaging_users_get_destinations($values = NULL) {
  global $user;
  static $destinations;
  
  if (isset($destinations)) {
    return $destinations;
  }
  
  $values = is_null($values) ? $_POST : $values;
  
  $destinations = array();

  // Get values from form form state 
  if (!empty($values['destinations'])) {
    if (is_array($values['destinations'])) {
      $destinations = $values['destinations'];
    } elseif (is_string($values['destinations'])) {
      $destinations = explode(',', $values['destinations']);
    }
  } 
  
  // Get values from sendto field and add if not duplicated
  if (!empty($values['sendto']) && is_string($values['sendto'])) {
    foreach (_messaging_users_match_multiple($values['sendto']) as $index => $dest) {
      $value = _messaging_users_check_destination($dest);
      if ($value && !in_array($value, $destinations)) {
        $destinations[] = $dest;
      }
    }
  }
  
  // Remove empty ones and return
  return array_filter($destinations);
}

/**
 * Store destinations in form store
 */
function _messaging_users_set_destinations($destinations) {

    $store = _messaging_users_form_store('get');
    if ($destinations) {
      $store['destinations'] = $destinations;
      _messaging_users_form_store('set', $store);
    } elseif ($store && !empty($store['destinations'])) {
      unset($store['destinations']);
      _messaging_users_form_store('set', $store);
    }
}

/**
 * Validate destinations, check permissions...
 */
function _messaging_users_validate_destinations($account, &$destinations, $remove = TRUE) {
  $errors = array();
  
  // Spam and permission control, we may need to filter out some destinations
  $limit = _messaging_users_can_send($account); // Limit for non friends

  // @TODO: probably something cleaner here.
  if (arg(3) == 'reply' && $limit == 0) {
    $limit = 1;
  }

  $sendspecial = user_access('send messages to all users', $account);
  $count = 0; // Keep a count of messages to non friends
  
  foreach ($destinations as $index => $to) {
    $valid = TRUE;
    $dest = _messaging_users_check_destination($to);
    if (!$dest) {
      $valid = FALSE;
      $errors['invalid'] = 1;    
    } elseif (is_numeric($dest)) {
      if ($limit >= 0 && !_messaging_users_is_friend($account->uid, $dest)) {
        $count++;
        if ($count > $limit) {
          $valid = FALSE;
          $errors['spam'] = $limit;
        }
      }
    } elseif (!$sendspecial) { // This is a special group destination
      $errors['special'] = 1;
      $valid = FALSE;
    }
    if (!$valid && $remove) unset($destinations[$index]);
  }
  // Set messages for each error
  foreach (array_keys($errors) as $type) {
    switch ($type) {
      case 'invalid':
        $msg = t('Invalid destinations.');
        break;
      case 'special':
        $msg = t("You don't have permission to send to special group destinations.");
        break;
      case 'spam':
        if (user_access('send messages to users', $account)) {
          $msg = t("You are allowed to post only %count messages to non friends an hour.", array('%count' => _messaging_users_spam_quota($account)));
        } else {
          $msg = t("You can only send messages to friends.");
        }
        break;
    }
    $errors[$type] = $msg;
  }
  return $errors;
}

/**
 * Get error messages
 */
function _messaging_users_error_messages($errors) {
  $messages = array();
  foreach ($errors as $type => $value) {
    
  }
  return $messages;
}

/**
 * Check destination format and return it as string/number
 */
function _messaging_users_check_destination($dest) {
  if (is_object($dest)) {
    return $dest->uid; // It is user account
  } elseif (is_numeric($dest)) {
    return (int)$dest; // This is user id
  } elseif (is_string($dest) && !empty($dest) && ($dest == check_plain($dest))) {
    return trim($dest);
  } else {
    return FALSE;
  }    
}

/**
 * Get hidden string field containing multiple destinations
 */
function _messaging_users_destinations2string($destinations) {
  // Convert to string/number and filter out empty ones
  $destinations = array_map('_messaging_users_check_destination', $destinations);
  $destinations = array_filter($destinations);
  return implode(',', $destinations);
}

/**
 * This one will return an friend count in destinations
 */
function _messaging_users_count_friends($uid, $destinations) {
  $count = 0;
  if ($friends = _messaging_users_get_friends($uid)) {
    foreach ($destinations as $dest) {
      $uid = is_object($dest) ? $dest->uid : $dest;
      $count += in_array($uid, $friends) ? 1 : 0;
    }
  }
  return $count;
}

/**
 * Form submission
 */
function messaging_users_compose_submit($form, &$form_state) {
  $op = isset($form_state['values']['op']) ? $form_state['values']['op'] : '';
  // Build message
  if ($op == t('Send')) {
    $form_state['redirect'] = 'user/' . $form_state['values']['sender']->uid . '/messages';
    $destinations = $form_state['values']['receivers'];

    if (messaging_message_send($destinations, $form_state['values']['message'] + array('sender' => $form_state['values']['sender']->uid), 'users')) {
      drupal_set_message(format_plural($message->count, 'The message has been sent.', '@count messages have been sent.'));
    } else {
      drupal_set_message(t('Error sending the message.'), 'error');
    }
  }
}

/**
 * Subform to render only a few controls, skip normal form rendering
 */
function messaging_users_destinations_subform($destinations = array(), $account = NULL, $ajax = FALSE) {
  global $user;
  
  $account = $account ? $account : $user;
  
  $form['#ajax_request'] = $ajax;


  if ($destinations) {
    //$form['list'] = array('#tree' => TRUE);
    $index = 0;
    foreach ($destinations as $dest) {
      $to = is_object($dest) ? $dest->uid : $dest;
      if (is_numeric($to)) {
        $type =  _messaging_users_is_friend($account->uid, $to) ? t('Friend') : t('User');
      } elseif (_messaging_users_special_destinations('key', $to)) {
        $type = t('Group');      
      } else {
        $type = t('Unknown');
      }
      $form['list']['dest'][$index] = array('#value' => $dest);
      $form['list']['value'][$index] = array('#value' => $to);
      $form['list']['name'][$index] = array('#value' => messaging_users_format_destination($dest));
      $form['list']['type'][$index] = array('#value' => $type);
      $form['list']['button'][$index] = array(
        '#type' => 'submit',
        '#value' => t('Remove'),
        '#ahah' => array(
          'path' => 'messaging_users/js/destinations/remove/' . $to,
          'wrapper' => 'messaging-destination-list',
          'type' => 'replace',         
        ),
        // Allow the user to remove only when they can compose.
        '#access' => messaging_users_access('compose'),
      );
      $index++;
    }
  }
  
  // This one will only be rendered for ajax requests
  if ($ajax) {
    $form['list']['destinations'] = array('#type' => 'hidden', '#value' => _messaging_users_destinations2string($destinations));
  }

  // Handle form rendering so we get only the code we want
  $form_id = 'messaging_users_destinations_subform';
  $form_state = array('storage' => NULL, 'submitted' => FALSE);
  drupal_prepare_form($form_id, $form, $form_state);
  $form = form_builder($form_id, $form, $form_state);

  return theme('messaging_users_destinations_subform', $form); 
}

/**
 * This will render just a subform, not the full form
 */
function theme_messaging_users_destinations_subform(&$form) {
  $output = '';
  
  if (!empty($form['list'])) {
    $rows = array();
    foreach (element_children($form['list']['dest']) as $index) {
      $rows[] = array(
        drupal_render($form['list']['name'][$index]),
        drupal_render($form['list']['type'][$index]),
        drupal_render($form['list']['button'][$index]),
        drupal_render($form['list']['test'][$index]),
      );
    }
    $output .= theme('table', array(), $rows);
  }

  // Add destinations and get ahah js, only for ajax requests
  if ($form['#ajax_request']) {
    $output .= drupal_render($form['list']['destinations']);
    $output .= '<script type="text/javascript">';
    $javascript = drupal_add_js(NULL, 'header');
    if ($javascript && !empty($javascript['setting'])) {
      // Magic line, taken from drupal_get_js
      $output .= 'jQuery.extend(Drupal.settings, ' . drupal_to_js(call_user_func_array('array_merge_recursive', $javascript['setting'])) . ");";
    } 
    // This will empty the sendto field
    $output .= '$("#edit-sendto").val(\'\')';
    $output .= "</script>\n";
  }
 
  return $output;
}

/**
 * Format destination list, ajax callback
 */
function messaging_users_destinations_list($op = 'add', $param = NULL) {
  global $user;
  $output = ''; 
  
  switch ($op) {
    case 'add':
      $destinations = _messaging_users_get_destinations();
      break;
    case 'clear':
      $destinations = array();
      break;
    case 'remove':
      $destinations = _messaging_users_get_destinations();
      $plaindests = array_map('_messaging_users_check_destination', $destinations);

      if ($param && is_numeric($index = array_search($param, $plaindests))) {
        unset($destinations[$index]);
      }
      break;
  }

  // Validate destinations if any and set error messages
  if ($destinations) {
      if ($errors = _messaging_users_validate_destinations($user, $destinations)) {
        foreach ($errors as $msg) {
          drupal_set_message($msg, 'error');
        }
      }    
  }

  // Store, there may be new destinations
  //_messaging_users_set_destinations($destinations);

  //$output .= print_r($destinations, TRUE);
  $output .= messaging_users_destinations_subform($destinations, $user, TRUE);
  // Add status messages  
  $output .= theme('status_messages', 'error');
  
  drupal_json(array('status' => TRUE, 'data' => $output));
}

/**
 * Match multiple destinations
 * 
 * @param $destination
 *   String of multiple destinations separated by commas
 * @return array
 *   Array of text destinations and user accounts
 */
function _messaging_users_match_multiple($destination) {
  $matches = array();
  
  foreach (split(',', $destination) as $name) {
    $name = trim($name);    
    if ($special = _messaging_users_special_destinations('name', $name)) {
      $matches[] = $special;
    } elseif (!user_validate_name($name) && ($account = user_load(array('name' => $name, 'status' => 1)))) {
      $matches[] = $account;
    }
  }
  return $matches;
}

/**
 * Ajax callback for message destination
 */
function messaging_users_autocomplete($fullstring = '') {
  global $user;

  $matches = array();  
  // The user enters a comma-separated list of destinations. We only autocomplete the last tag.
  if ($fullstring) {
    $array = split(',', $fullstring);
    $string = trim(array_pop($array));
    
    // Check single user names
    if ($string) {
      $prefix = count($array) ? implode(', ', $array) .', ' : '';
      if (user_access('send messages to users')) {
        $result = db_query_range("SELECT name FROM {users} WHERE name LIKE '%s%%'", $string, 0, 10);
      }
      elseif (user_access('send messages to friends') && module_exists('flag_friend')) {
        $result = db_query_range("SELECT u.name FROM {users} u INNER JOIN {flag_friend} f1 ON u.uid = f1.uid INNER JOIN {flag_friend} f2 ON u.uid = f2.friend_uid WHERE (f1.friend_uid = %d OR f2.uid = %d) AND u.name LIKE '%s%%'", $user->uid, $user->uid, $string, 0, 10);
      } 
      if ($result) {
        while ($userto = db_fetch_object($result)) {
          $matches[$prefix . $userto->name] = $prefix . check_plain($userto->name);
        }
      }
      // If bulk send permissions, these will override the others
      // @todo This may not work for non utf-8 texts
      if (user_access('send messages to all users')) {
        foreach (_messaging_users_special_destinations() as $key => $name) {
          if (stripos($name, $string) === 0) {
            $matches[$prefix . $name] = $prefix . $name;
          }
        }
      }
    }
  }
  drupal_json($matches); 
}

/**
 * settings form
 */
function messaging_users_settings_form(&$form_state = NULL, $account) {
  $subscriptions = notifications_get_subscriptions(array('uid' => $account->uid, 'type' => 'messaging'), array('method' => 'users'));
  $subscription = current($subscriptions);
  $form['#subscription'] = $subscription;
  $form['#account'] = $account;
  // display any markup?
  $form['setting'] = array(
    '#prefix' => '<div class="container-inline">',
    '#type' => 'select',
    '#options' => array(0 => 'Yes', -1 => 'No'),
    '#title' => t('Would you like to be notified of new messages by Mail?'),
    '#default_value' => $subscription->send_interval,
    '#suffix' => '</div>',
  );
  
  $form['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  return $form;
}

function messaging_users_settings_form_submit(&$form, &$form_state) {
  $sub = $form['#subscription'];
  // in case the user doesn't have a subscription create a new one
  if (!$sub) {
    $account = $form['#account'];
    $sub = array(
      'sid' => 0,
      'type' => 'messaging',
      'event_type' => 'message',
      'send_method' => notifications_user_setting('send_method', $account),
      'send_interval' => $form_state['values']['setting'],
      'fields' => array('method' => 'users'),
      'uid' => $account->uid,
      'module' => 'messaging',
    );
  }
  else {
    $sub->send_interval = $form_state['values']['setting'];
  }
  notifications_save_subscription($sub);
  drupal_set_message(t('Your settings have been saved'));
}

/**
 * Reader form
 */
function messaging_users_reader(&$form_state, $account, $type, $messages, $header, $operations) {
  $form['account'] = array('#type' => 'value', '#value' => $account);
  $form['type'] = array('#type' => 'value', '#value' => $type);
  $form['#table_header'] = $header;
  $form['#table_fields'] = array('subject', 'name', 'date');

  $form['options'] = array(
    '#type' => 'fieldset',
    '#title' => t('With selected messages...'),
  );
  $form['options']['operation'] = array(
    '#type' => 'select',
    '#options' => $operations,
    '#default_value' => 'delete',
    '#prefix' => '<div class="container-inline">',
  );
  $form['options']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Update'),
    '#suffix' => '</div>',
  );

  $status = array(t('Unread'), t('Read'));
  foreach ($messages as $msg)   {
    $sender = user_load($msg->uid);
    $mqids[$msg->mqid] = '';
    $form['subject'][$msg->mqid] = array('#value' => l($msg->subject, 'user/' . $account->uid . '/messages/' . $type . '/' . $msg->mqid));
    if ($type == 'inbox') {
      $picture_path = $sender->picture;
      if (empty($picture_path) || !file_exists($picture_path)) {
        $picture_path = variable_get('user_picture_default', '');
      }
      $form['name'][$msg->mqid] = array('#value' => $picture_path, '#theme' => 'messaging_users_picture', '#account' => $sender);
    } else {
      // If more than 2 recipients provide a more link which displays full message.
      if (count($msg->destinations) > 2) {
        $more = ', ' . l('more...', 'user/' . $account->uid . '/messages/' . $type . '/' . $msg->mqid);
        $form['name'][$msg->mqid] = array('#value' => messaging_users_format_receivers($msg->destinations, 'line', 2) . $more);
      }
      else {
        $picture_path = user_load($msg->destinations[0])->picture;
        if (empty($picture_path) || !file_exists($picture_path)) {
          $picture_path = variable_get('user_picture_default', '');
        }
        $form['name'][$msg->mqid] = array('#value' => $picture_path, '#theme' => 'messaging_users_picture', '#account' => user_load($msg->destinations[0]));
      }
    }
    $form['date'][$msg->mqid] = array('#value' => format_date($msg->sent));
    $form['status'][$msg->mqid] = array('#read' => $msg->status);
  }
  $form['messages'] = array(
    '#type' => 'checkboxes',
    '#options' => $mqids,
  );
  $form['pager'] = array('#value' => theme('pager', NULL, 50, 0));
  // Different theme for inbox /outbox
  $form['#theme'] = 'messaging_users_reader_' . $type;
  return $form;
}

/**
 * Submit reader form, bulk operations
 */
function messaging_users_reader_submit($form, &$form_state) {
  $type = $form_state['values']['type'];
  $messages = array_filter($form_state['values']['messages']);
  $account = $form_state['values']['account'];
  $updated = $deleted = 0;
  
  if ($messages) {
    switch ($form_state['values']['operation']) {
      case 'read':
        messaging_users_bulk_update($type, $account->uid, $messages, MESSAGING_USERS_STATUS_READ);
        drupal_set_message(format_plural(count($messages), 'The message has been updated.', '@count messages have been updated.'));
        break;
      case 'delete':
        messaging_users_bulk_update($type, $account->uid, $messages, MESSAGING_USERS_STATUS_DELETE);
        drupal_set_message(format_plural(count($messages), 'The message has been deleted.', '@count messages have been deleted.'));
        break;      
    }
  }
}

function messaging_users_delete($account, $message) {
  $type = 'inbox';
  messaging_users_bulk_update($type, $account->uid, $message->mqid, MESSAGING_USERS_STATUS_DELETE);
  drupal_set_message(format_plural(count($message), 'The message has been deleted.', '@count messages have been deleted.'));
  drupal_goto('user/'. $account->name .'/messages/inbox');
}

/**
 * Bulk update, execute query on all messages
 */
function messaging_users_bulk_update($type, $uid, $mqids, $status) {
  $mqids = is_array($mqids) ? $mqids : array($mqids);
  $args = array_merge(array($status, $uid), $mqids);
  
  if ($type == 'inbox') {
    $sql = 'UPDATE {messaging_users} SET status = %d WHERE receiver = %d';
  } else {
    $sql = 'UPDATE {messaging_store} SET queue = %d WHERE sender = %d'; 
  }
  return db_query($sql . ' AND mqid IN (' . db_placeholders($mqids, 'int') . ')', $args );
}

/**
 * Admin settings form
 */
function messaging_users_admin_settings() {
  $settings = variable_get('messaging_users_spam_role', array());
  $form['messaging_users_spam_role'] = array(
    '#type' => 'fieldset',
    '#title' => t('Spam control'),
    '#description' => t("Limit the hourly number of messages for sending to non friends per role. This won't apply to roles with the 'send to all users' permission. Zero (0) means no limit."),
    '#tree' => TRUE,
  );
  foreach (user_roles() as $rid => $name) {
    if (!in_array($rid, array(DRUPAL_ANONYMOUS_RID))) {
      $form['messaging_users_spam_role'][$rid] = array(
        '#type' => 'textfield',
        '#title' => $name,
        '#default_value' => isset($settings[$rid]) ? $settings[$rid] : 0,
        '#size' => 10,
      );
    }
  }
  // Set up ignore flag
  if (module_exists('flag')) {
    $form['flag'] = array(
      '#type' => 'fieldset',
      '#title' => t('Flag settings'),
    );
    // Build a list of available non global user flags
    $available = array('' => '');
    foreach (flag_get_flags('user') as $flag) {
      if (!$flag->global) {
        $available[$flag->fid] = $flag->title;
      }
    }
    $form['flag']['messaging_users_ignore_flag'] = array(
      '#type' => 'select',
      '#title' => t('Ignore flag'),
      '#options' => $available,
      '#default_value' => variable_get('messaging_users_ignore_flag', ''),
      '#description' => t('Flag to use for not accepting messages from user. These are custom flags of type User and not Global.'),
    );
  }
  $form['messaging_users_subject'] = array(
    '#type' => 'textfield',
    '#title' => t('Welcome message subject'),
    '#default_value' => variable_get('messaging_users_subject', ''),
    '#description' => t('The subject of the welcome private message'),
  );
  $form['messaging_users_welcome'] = array(
    '#type' => 'textarea',
    '#title' => t('Welcome Message'),
    '#default_value' => variable_get('messaging_users_welcome', ''),
    '#description' => t('The message to send to new users when they sign up'),
  );
  $name = db_result(db_query("SELECT name FROM {users} WHERE uid = %d", variable_get('messaging_users_welcome_sender', 18)));
  $form['messaging_users_welcome_sender'] = array(
    '#type' => 'textfield',
    '#title' => t('Sender'),
    '#default_value' => $name,
    '#description' => t('The sender of the message'),
    '#autocomplete_path' => 'user/autocomplete',
  );
  $form['#validate'][] = 'messaging_users_admin_settings_validate';
  return system_settings_form($form);
}

/**
 * Validate the admin settings form, setting the proper form value
 */
function messaging_users_admin_settings_validate($form, &$form_state) {
  $uid = db_result(db_query("SELECT uid FROM users WHERE name = '%s'", $form_state['values']['messaging_users_welcome_sender']));
  form_set_value($form['messaging_users_welcome_sender'], $uid, $form_state);
}

/**
 * Theme message reader form
 */
function theme_messaging_users_reader_inbox(&$form) {
  // Overview table:
  $header = $form['#table_header'];
  $header[0] = theme('table_select_header_cell');

  $output = drupal_render($form['options']);

  foreach (element_children($form['subject']) as $key) {
    $row['class'] = $form['status'][$key]['#read'] ? 'unread' : 'read';
    $row['data'] = array(
      drupal_render($form['messages'][$key]),
      drupal_render($form['name'][$key]), 
      drupal_render($form['subject'][$key]),       
      drupal_render($form['date'][$key]),
      //drupal_render($form['status'][$key]),
    );
    $rows[] = $row;
  }

  $output .= theme('table', $header, $rows);
  
  if ($form['pager']['#value']) {
    $output .= drupal_render($form['pager']);
  }

  $output .= drupal_render($form);

  return $output; 
}

/**
 * Theme message reader form
 */
function theme_messaging_users_reader_outbox(&$form) {
  // Overview table:
  $header = $form['#table_header'];
  $header[0] = theme('table_select_header_cell');

  $output = drupal_render($form['options']);

  foreach (element_children($form['subject']) as $key) {
    $row['class'] = 'read'; // hardcoded
    $row['data'] = array(
      drupal_render($form['messages'][$key]),
      drupal_render($form['name'][$key]),
      drupal_render($form['subject'][$key]),       
      drupal_render($form['date'][$key]),
    );
    $rows[] = $row;
  }

  $output .= theme('table', $header, $rows);
  
  if ($form['pager']['#value']) {
    $output .= drupal_render($form['pager']);
  }

  $output .= drupal_render($form);

  return $output; 
}

/**
 * Format destination table for compose page
 */
function theme_messaging_users_destinations_table($destinations, $account = NULL) {
  global $user;
  
  $account = $account ? $account : $user;
  
  foreach ($destinations as $dest) {
    $to = is_object($dest) ? $dest->uid : $dest;
    $picture = '';
    if (is_numeric($to)) {
      $type = _messaging_users_is_friend($account->uid, $to) ? t('Friend') : t('User');
    } elseif (_messaging_users_special_destinations('key', $to)) {
      $type = t('Group');      
    } else {
      $type = t('Unknown');
    }
    // @todo Add user pictures here, maybe for all, maybe only for friends
    $rows[] = array(
      messaging_users_format_destination($dest),
      $type,
      $picture,
    );
  }
  return theme('table', array(), $rows); 
}

function theme_messaging_users_destinations_list($destinations) {
  // Format for display
  $items = array_map('messaging_users_format_destination', $destinations);
  $form['list'] = array('#value' => theme('item_list', $items), '#weight' => -10);
  $output .= theme('item_list', $items);
  return $output;
}

function theme_messaging_users_message_preview($message) {
  $output = '';
  if (!empty($message['subject'])) {
    $output .= '<div class="message-subject"><strong>' . $message['subject'] . '</strong></div>';
    
  }
  if (!empty($message['body'])) {
    $filter = messaging_method_info('users', 'filter');
    $output .= '<div class="message-body">' . check_markup($message['body'], $filter) . '</div>';    
  }
  return $output;
}

function theme_messaging_users_page_read($form) {
  if (isset($form['from'])) {
    $form['from']['#prefix'] = '<div class="message-from">';
    $form['from']['#suffix'] = '</div>';
  }
  
  if (isset($form['to'])) {
    $form['to']['#prefix'] = '<div class="message-to">';
    $form['to']['#suffix'] = '</div>';
  }
  
  $form['subject']['#prefix'] = '<div class="message-subject">';
  $form['subject']['#suffix'] = '</div>';
  
  $form['content']['#prefix'] = '<div class="message-content">';
  $form['content']['#suffix'] = '</div>';
  
  if (isset($form['reply']) || isset($form['delete'])) {
    $form['buttons'] = array(
      '#type' => 'fieldset',
      '#collapible' => FALSE,
      '#weight' => 42,
    );
  }
  $form['buttons']['reply'] = $form['reply'];
  $form['buttons']['delete'] = $form['delete'];
  
  unset($form['reply']);
  unset($form['delete']);
  
  return '<div id="message">'. drupal_render($form) .'</div>';
}

function theme_messaging_users_page_read_links($form) {
  $output = '<div class="button">'. l($form['#title'], $form['#value'], array('attributes' => array('class' => $form['#title'] .' button'))) .'</div>';
  return $output;
}

function theme_messaging_users_picture($element) {
  if (isset($element['#imagecache_preset'])) {
    $image = theme('imagecache', $element['#imagecache_preset'], $element['#value'], $element['#title'], $element['#alt']);
  }
  else {
    $image = theme('image', $element['#value'], $element['#title'], $element['#alt']);
  }

	return '<div class="picture">' . l($image, 'user/' . $element['#account']->uid, array('html' => TRUE)) . '</div><div class="username">' . l(user_profile_get_name($element['#account']), 'user/' . $element['#account']->uid) . '</div>';
}