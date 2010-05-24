<?php

function _messaging_users_block_user_pm_flags(&$flags) {
  $flags[] = array(
    'content_type' => 'user',
    'name' => 'block_user_pm',
    'title' => 'Block user PM',
    'roles' => array(
      0 => '2',
    ),
    'global' => FALSE,
    'flag_short' => 'Block this user from sending you messages.',
    'flag_confirmation' => 'Are you sure you want to block [user] from sending you messages?',
    'flag_message' => 'Blocked',
    'unflag_short' => 'Unblock this user from sending you messages.',
    'unflag_confirmation' => 'Are you sure you want to unblock [user] from sending you messages?',
    'unflag_message' => 'Unblocked',
    'status' => FALSE,
    'show_on_teaser' => FALSE,
    'link_type' => 'confirm',
    'locked' => array('name', 'global'),
  );
  return $flags;
}