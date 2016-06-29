<?php
/**
 * @file
 * Administrative interface for Meta link module.
 */

/**
 * Renders admin form matching algorithm results.
 *
 * @submit meta_link_run_algorithm_submit.
 *
 * @submit meta_link_accept_matches_submit.
 */
function meta_link_admin_form($form, &$form_state) {
	$form['#tree'] = TRUE;
	// getting array of matches.
	$matches = _meta_link_get_matches();
	if (isset($matches) && !empty($matches)) {
		 $table = array(
      '#prefix' => '<table>',
      '#suffix' => '</table>',
    );
		$header = array(
			'#prefix' => '<thead>',
			'#suffix' => '</thead>',
		);
		$header['match_id'] = array('#markup' => '<th>' . t('Accept Match') . '</th>');
		$header['record_title'] = array('#markup' => '<th>' . t('Record Title') . '</th>');
		$header['repository_source'] = array('#markup' => '<th>' . t('Source Repository') . '</th>');
		$header['recommended_work'] = array('#markup' => '<th>' . t('Suggested Work Reference') . '</th>');
		$header['algorithm_step'] = array('#markup' => '<th>' . t('Algorithm Step') . '</th>');
		$header['match_score'] = array('#markup' => '<th>' . t('Match Score') . '</th>');
		$table['header'] = $header;
		$rows = array('#prefix' => '<tbody>', '#suffix' => '</tbody>');
		foreach($matches as $k => $match) {
			if ($k % 2 == 0) {
				$class = 'even';
			}
			else {
				$class = 'odd';
			}
			$rows[$match->id]['match_id'] = array(
				'#type' => 'checkbox',
        '#title' => $match->id,
        '#title_display' => 'attribute',
        '#default_value' => 0,
        '#prefix' => '<tr class="' . $class . '"><td>',
        '#suffix' => '</td>',
      );
      $rows[$match->id]['record_title'] = array('#markup' => '<td>' . l(t('@title', array('@title' => explode("|", $match->title_array)[0])), '/node/' . $match->record_id) . '</td>');
      $rows[$match->id]['field_repository_source_value'] = array('#markup' => '<td>' . $match->field_repository_source_value . '</td>');
      $rows[$match->id]['recommended_work'] = array('#markup' => '<td>' . l(t('@title', array('@title' => $match->title)), '/node/' . $match->recommended_work_id) . '</td>');
      $rows[$match->id]['algorithm_step'] = array('#markup' => '<td>' . $match->algorithm_step . '</td>');
      $rows[$match->id]['match_score'] = array('#markup' => '<td>' . $match->match_score . '</td></tr>');
		}

		$table['fields'] = $rows;
		$form['matches'] = $table;
		$form['#suffix'] =  theme('pager', array('quantity',count($rows)));
		$form['actions']['accept_matching'] = array(
			'#type' => 'submit',
			'#value' => t('Accept Suggested Matches'),
			'#submit' => array('meta_link_accept_matches_submit'),
		);
		$form['actions']['reject_matching'] = array(
			'#type' => 'submit',
			'#value' => t('Reject Suggested Matches'),
			'#submit' => array('meta_link_reject_matches_submit'),
		);
		$form['actions']['clear_rejections'] = array(
			'#type' => 'submit',
			'#value' => t('Clear Rejected Flags'),
			'#submit' => array('meta_link_clear_rejected_submit'),
		);
	}
	else {
		$form['matches'] = array('#markup' => '<div>There are currently no matches. Please run the matching algorithm by clicking the button below.</div>');
	}

	$form['actions']['run_matching'] = array(
		'#type' => 'submit',
		'#value' => t('Run Matching Algorithm'),
		'#submit' => array('meta_link_run_algorithm_submit'),
	);

	return $form;
}

/**
 * Submit handler for Matching Algorithm form.
 *
 * @see meta_link_admin_form.
 */
function meta_link_accept_matches_submit($form, &$form_state) {
	if (isset($form_state['values']['matches']['fields'])) {
		$matches_ids = array();
		foreach ($form_state['values']['matches']['fields'] as $id => $match) {
			if ($match['match_id']) {
				$matches_ids[] = $id;
			}
		}
		$batch = _meta_link_accept_match_batch_init($matches_ids);
		batch_set($batch);
	}
}


/**
 * Submit handler for Matching Algorithm form.
 *
 * @see meta_link_admin_form.
 */
function meta_link_reject_matches_submit($form, &$form_state) {
	if (isset($form_state['values']['matches']['fields'])) {
		$matches_ids = array();
		foreach ($form_state['values']['matches']['fields'] as $id => $match) {
			if ($match['match_id']) {
				$matches_ids[] = $id;
			}
		}
		$batch = _meta_link_reject_match_batch_init($matches_ids);
		batch_set($batch);
	}
}

/**
 * Submit handler for Matching Algorithm form.
 *
 * @see meta_link_admin_form.
 */
function meta_link_clear_rejected_submit($form, &$form_state) {
	$query = db_update('meta_link_data')
				 ->fields(array('is_rejected' => NULL))
				 ->execute();
}
/**
 * Submit handler for Matching Algorithm form.
 *
 * @see meta_link_admin_form.
 */
function meta_link_run_algorithm_submit($form, &$form_state) {
	// making a match suggestion in batch.
	$batch = _init_matches_batch();
	batch_set($batch);
}
