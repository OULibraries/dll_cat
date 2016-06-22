<?php
/**
 * @file
 * Administrative interface for Archive meta import module.
 */


/**
 * Renders admin form for editor to enter urls of repositories.
 */
function archive_meta_import_admin_form($form, &$form_state) {
	$default_urls = variable_get('meta_repo_urls');
	if (isset($default_urls) && !empty($default_urls)) {
		foreach ($default_urls as $key => $url) {
			if ($url['url'] == '') {
				unset($default_urls[$key]);
			}
		}
		$default_urls = array_values($default_urls);
		$form_state['storage']['meta_repo_urls'] = isset($form_state['storage']['meta_repo_urls']) ? $form_state['storage']['meta_repo_urls'] : count($default_urls)-1;
	}
	else {
		$form_state['storage']['meta_repo_urls'] = isset($form_state['storage']['meta_repo_urls']) ? $form_state['storage']['meta_repo_urls'] : 0;
	}

	$form['meta_repo_urls'] = array(
		'#type' => 'fieldset',
		'#title' => t('Repositories URLs'),
		'#tree' => TRUE,
		'#prefix' => '<div id="urls">',
		'#suffix' => '</div>',
		'#description' => t('These repositories URLs will be used during the next scheduled import.'),
	);
	if ($form_state['storage']['meta_repo_urls'] !== NULL) {
		for ($i = 0; $i <= $form_state['storage']['meta_repo_urls']; $i++) {
			$form['meta_repo_urls'][$i] = array(
				'#type' => 'container',
				'#tree' => TRUE,
			);

			$form['meta_repo_urls'][$i]['url'] = array(
				'#title' => t('Repository URL'),
				'#title_display' => 'invisible',
				'#type' => 'textfield',
				'#default_value' => isset($default_urls[$i]) ? $default_urls[$i] : '',
				'#description' => isset($default_urls[$i]['url']) ? '<strong>Domain: </strong>' . parse_url($default_urls[$i]['url'])['host'] : '',
			);
		}
	}

	$form['meta_repo_urls']['add_url'] = array(
		'#type' => 'button',
		'#value' => t('Add another repository URL'),
		'#href' => '',
		'#ajax' => array(
			'callback' => 'archive_meta_import_ajax',
			'wrapper' => 'urls',
		),
	);

	$form_state['storage']['meta_repo_urls']++;

	$form['actions']['schedule_import'] = array(
		'#type' => 'button',
		'#value' => t('Schedule Import'),
		'#executes_submit_callback' => TRUE,
	);

	$form['#submit'][] = 'archive_meta_import_schedule_import';

	return system_settings_form($form);
}

/**
 * Submit callback for the import configuration form.
 *
 * @see archive_meta_import_admin_form().
 */
function archive_meta_import_schedule_import($form,  &$form_state) {
	if ($form_state['clicked_button']['#value'] == 'Schedule Import') {
		variable_set('meta_import_scheduled', TRUE);
		drupal_set_message(t('Import was scheduled.'), 'status', FALSE);
	}
}

/**
 * AJAX callback function
 */
function archive_meta_import_ajax($form, $form_state) {
	return $form['meta_repo_urls'];
}