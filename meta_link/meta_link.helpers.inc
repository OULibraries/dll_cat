<?php
/**
 * @file
 * Helper functions
 */

/**
 * Helper function to get all made to date matches.
 *
 * @return array res
 *		result array of matches.
 */
function _meta_link_get_matches() {
	$query = db_select('node', 'n');
	$query->join('meta_link_data', 'matches', 'n.nid = matches.record_id');
	$query->join('field_data_field_record_title', 'title', 'title.entity_id = n.nid');
	$query->join('field_data_field_repository_source', 'source_domain', 'source_domain.entity_id = n.nid');
	$query->leftJoin('node', 'work', 'work.nid = matches.recommended_work_id');
	$query->leftJoin('field_data_field_work_reference', 'ref', 'n.nid = ref.entity_id');
	$query->addExpression("GROUP_CONCAT( DISTINCT title.field_record_title_value SEPARATOR '|' )", 'title_array');
	$query->fields('matches', array('id', 'record_id', 'recommended_work_id', 'algorithm_step', 'match_score'))
		    ->fields('source_domain', array('field_repository_source_value'))
		    //->fields('title', array('field_record_title_value'))
		    ->fields('work', array('title'));
	$query->condition('n.type', 'metadata');
	$query->condition('ref.field_work_reference_target_id', NULL, 'IS');
	$query->condition('matches.is_rejected', NULL, 'IS');
	$query->groupBy('matches.id');
	$query->orderBy('matches.algorithm_step', 'ASC');
	$query->orderBy('matches.match_score', 'DESC');
	$query = $query->extend('PagerDefault')->limit(25);
	$res = $query->execute()->fetchAll();
	return $res;
}

/**
 * Helper function to truncate matches table.
 *
 * TODO: should we do it in a batch?
 */
function _meta_link_delete_matches() {
	$query = db_delete('meta_link_data')
		->condition('meta_link_data.is_rejected', NULL, 'IS')
		->execute();
}

/**
 * Helper function to initialize batch process for matching accepting algorithm.
 */
function _meta_link_accept_match_batch_init($matches_ids) {
	$operations = array();
	$operations[] = array('_meta_link_work_link_accept', array($matches_ids));
	$operations[] = array('_meta_link_delete_accepted_matches', array());
	$batch = array(
		'title' => t('Accepting matches'),
		'operations' => $operations,
	);
	return $batch;
}

function _meta_link_reject_match_batch_init($matches_ids) {
	$operations = array();
	$operations[] = array('_meta_link_mark_as_rejected', array($matches_ids));
	$batch = array(
		'title' => t('Rejecting matches'),
		'operations' => $operations,
	);
	return $batch;

}

/**
 * Helper function to initialize batch process for the matching algorithm.
 */
function _init_matches_batch() {
	$operations = array();
	$operations[] = array('_meta_link_delete_matches', array());
	$operations[] = array('_meta_link_get_all_records', array());
	$operations[] = array('_meta_link_look_up_primary', array());
	$operations[] = array('_meta_link_check_soundex', array());
	$operations[] = array('_meta_link_work_make_soundex', array());
	$operations[] = array('_meta_link_make_records_soundex', array());
	$operations[] = array('_meta_link_check_distance', array());
	$operations[] = array('_meta_link_save_match_results', array());
	$batch = array(
	  'title' => t('Making matches to works'),
	  'operations' => $operations,
	);
	return $batch;
}


/**
 * Helper function to get all records from db.
 */
function _meta_link_get_all_records(&$context) {
	$query = db_select('node', 'n');
	$query->join('field_data_field_record_title', 'title', 'title.entity_id = n.nid');
	$query->fields('n', array('nid'));
	$query->addExpression("GROUP_CONCAT( DISTINCT title.field_record_title_value SEPARATOR '|' )", 'title_array');
	//$query->fields('title', array('field_record_title_value'));
	$query->condition('n.type', 'metadata');
	$query->groupBy('n.nid');
	$res = $query->execute()->fetchAll();
	if (isset($res) && !empty($res)) {
		$context['results']['records'] = $res;
	}
}

/**
 * Helper function to try find a matching work by plain LIKE query on the title.
 */
function _meta_link_look_up_primary(&$context) {
	if (isset($context['results']['records'])) {
		$limit = 1;
		if (empty($context['sandbox'])) {
			$context['sandbox']['progress'] = 0;
			$context['sandbox']['current_record'] = 0;
			$context['sandbox']['max'] = count($context['results']['records']);
			$context['sandbox']['offset'] = 0;
		}
		else {
			$context['sandbox']['offset'] = $context['sandbox']['offset'] + $limit + 1;
		}

		$off = min($context['sandbox']['offset'] + $limit, $context['sandbox']['max'] - 1);
		for ($i = $context['sandbox']['offset']; $i <= $off; $i++) {
			// lookup query
			$work_match = _meta_link_primary_query($context['results']['records'][$i]->title_array);
			if (isset($work_match) && !empty($work_match)) {
				$context['results']['records'][$i]->algorithm_step = 1;
				$context['results']['records'][$i]->work_match = $work_match;
				if ($work_match->score == 0) {
					$context['results']['records'][$i]->match_score = 1;
				}
				else {
					$context['results']['records'][$i]->match_score = $work_match->score * 0.5;
				}
			}
			else {
				$context['results']['records'][$i]->algorithm_step = 1;
				$context['results']['records'][$i]->match_score = 0;
			}
			$context['sandbox']['progress']++;
			$context['message'] = t('Now matching nid %nid', array('%nid' => $context['results']['records'][$i]->nid));
		}

		if ($context['sandbox']['progress'] < $context['sandbox']['max']) {
			$context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
		}
		else {
			$context['sandbox'] = array();
			$context['finished'] = 1;
		}
	}
}

/**
 * Helper function to try find a match by soundex.
 */
function _meta_link_check_soundex(&$context) {
	if (isset($context['results']['records'])) {
		$limit = 1;
		if (empty($context['sandbox'])) {
			$context['sandbox']['progress'] = 0;
			$context['sandbox']['current_record'] = 0;
			$context['sandbox']['max'] = count($context['results']['records']);
			$context['sandbox']['offset'] = 0;
		}
		else {
			$context['sandbox']['offset'] = $context['sandbox']['offset'] + $limit + 1;
		}

		$off = min($context['sandbox']['offset'] + $limit, $context['sandbox']['max'] - 1);
		for ($i = $context['sandbox']['offset']; $i <= $off; $i++) {
			if (!isset($context['results']['records'][$i]->work_match)) {
				$title_arr = $context['results']['records'][$i]->title_array;
				$title_arr = explode("|", $title_arr);
				if (!empty($title_arr)) {
					$work_match = NULL;
					$best_score = 1000;
					foreach($title_arr as $title) {
						$work_match_raw = _meta_link_soundex_query($title);
						if (isset($work_match_raw) && is_object($work_match_raw) && $work_match_raw->score < $best_score) {
							$work_match = $work_match_raw;
							$best_score = $work_match_raw->score;
						}
					}
				}

				if (isset($work_match) && !empty($work_match)) {
					$context['results']['records'][$i]->algorithm_step = 2;
					$context['results']['records'][$i]->work_match = $work_match;
					if ($work_match->score == 0) {
						$context['results']['records'][$i]->match_score = 1;
					}
					else {
						$context['results']['records'][$i]->match_score = $work_match->score * 0.5;
					}
				}
				else {
					$context['results']['records'][$i]->algorithm_step = 2;
					$context['results']['records'][$i]->match_score = 0;
				}
			}
			$context['sandbox']['progress']++;
			$context['message'] = t('Now matching nid %nid using soundex', array('%nid' => $context['results']['records'][$i]->nid));
		}

		if ($context['sandbox']['progress'] < $context['sandbox']['max']) {
			$context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
		}
		else {
			$context['sandbox'] = array();
			$context['finished'] = 1;
		}
	}
}


/**
 * Helper funciton to convert all work titles that are currently in the db into the soundex codes.
 */
function _meta_link_work_make_soundex($context) {
	$limit = 1;
	if (empty($context['sandbox'])) {
		$works = _meta_link_get_all_works();
		if (isset($works) && !empty($works)) {
			$context['sandbox']['progress'] = 0;
			$context['sandbox']['current_record'] = 0;
			$context['sandbox']['max'] = count($works);
			$context['sandbox']['offset'] = 0;
			$context['results']['works'] = $works;
		}
		else {
			$context['finished'] = 1;
			return;
		}
	}
	else {
		$context['sandbox']['offset'] = $context['sandbox']['offset'] + $limit + 1;
	}
	$off = min($context['sandbox']['offset'] + $limit, $context['sandbox']['max'] - 1);
	for ($i = $context['sandbox']['offset']; $i <= $off; $i++) {
		$sub_t = substr($context['results']['works'][$i]->title,  0, 254);
		if ($sub_t) {
			$context['results']['works'][$i]->soundex_title = soundex($sub_t);
		}
		else {
			$context['results']['works'][$i]->soundex_title = soundex($context['results']['works'][$i]->title);
		}


		$context['sandbox']['progress']++;
		$context['message'] = t('Calculating Soundex');
	}

	if ($context['sandbox']['progress'] < $context['sandbox']['max']) {
		$context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
	}
	else {
		$context['sandbox'] = array();
		$context['finished'] = 1;
	}
}

/**
 * Helper function to convert all unmatched record titles into soundex codes.
 */
function _meta_link_make_records_soundex($context) {
	if (isset($context['results']['records'])) {
		$limit = 1;
		if (empty($context['sandbox'])) {
			$context['sandbox']['progress'] = 0;
			$context['sandbox']['current_record'] = 0;
			$context['sandbox']['max'] = count($context['results']['records']);
			$context['sandbox']['offset'] = 0;
		}
		else {
			$context['sandbox']['offset'] = $context['sandbox']['offset'] + $limit + 1;
		}

		$off = min($context['sandbox']['offset'] + $limit, $context['sandbox']['max'] - 1);
		for ($i = $context['sandbox']['offset']; $i <= $off; $i++) {
			if (!isset($context['results']['records'][$i]->work_match)) {
				$title_array = explode("|", $context['results']['records'][$i]->title_array);
				$sub_t = substr($title_array[0], 0, 254);
				if ($sub_t) {
					$context['results']['records'][$i]->title_soundex = soundex($sub_t);
				}
				else {
					$context['results']['records'][$i]->title_soundex = soundex($context['results']['records'][$i]->title_array[0]);
				}
			}
			$context['sandbox']['progress']++;
			$context['message'] = t('Calculating record soundex');
		}

		if ($context['sandbox']['progress'] < $context['sandbox']['max']) {
			$context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
		}
		else {
			$context['sandbox'] = array();
			$context['finished'] = 1;
		}
	}
}


/**
 * Helper function to check levenshtein distance between 2 soundex codes.
 */
function _meta_link_check_distance($context) {
	if (isset($context['results']['records']) && isset($context['results']['works'])) {
		$limit = 1;
		// levenshtein distance limit
		// 1 is very fuzzy.
		$lev_limit = 0;
		if (empty($context['sandbox'])) {
			$context['sandbox']['progress'] = 0;
			$context['sandbox']['current_record'] = 0;
			$context['sandbox']['max'] = count($context['results']['records']);
			$context['sandbox']['offset'] = 0;
		}
		else {
			$context['sandbox']['offset'] = $context['sandbox']['offset'] + $limit + 1;
		}
		$off = min($context['sandbox']['offset'] + $limit, $context['sandbox']['max'] - 1);
		for ($i = $context['sandbox']['offset']; $i <= $off; $i++) {
			if (!isset($context['results']['records'][$i]->work_match)) {
				$current_dist = $lev_limit + 1;
				foreach ($context['results']['works'] as $key => $work) {
					$dist = levenshtein($work->soundex_title, $context['results']['records'][$i]->title_soundex);
					if ($dist !== -1 && $dist <= $lev_limit && $dist < $current_dist) {
						$current_dist = $dist;
						$context['results']['records'][$i]->work_match = $work;
						$context['results']['records'][$i]->algorithm_step = 3;
						if ($dist != 0) {
							$context['results']['records'][$i]->match_score = 0.25/($dist/0.5);
						}
						else {
							$context['results']['records'][$i]->match_score = 0.25*0.5;
						}
					}
					else {
						if (!isset($context['results']['records'][$i]->work_match)) {
							$context['results']['records'][$i]->match_score  = 0;
							$context['results']['records'][$i]->algorithm_step = 3;
						}
					}
				}
			}
			$context['sandbox']['progress']++;
			$context['message'] = t('Checking Levenshtein distance');
		}

		if ($context['sandbox']['progress'] < $context['sandbox']['max']) {
			$context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
		}
		else {
			$context['sandbox'] = array();
			$context['finished'] = 1;
		}
	}
}


/**
 * Helper function to save found matches.
 */
function _meta_link_save_match_results($context) {
	if (isset($context['results']['records'])) {
		$limit = 1;
		if (empty($context['sandbox'])) {
			$context['sandbox']['progress'] = 0;
			$context['sandbox']['current_record'] = 0;
			$context['sandbox']['max'] = count($context['results']['records']);
			$context['sandbox']['offset'] = 0;
		}
		else {
			$context['sandbox']['offset'] = $context['sandbox']['offset'] + $limit + 1;
		}

		$off = min($context['sandbox']['offset'] + $limit, $context['sandbox']['max'] - 1);
		for ($i = $context['sandbox']['offset']; $i <= $off; $i++) {
			$record = array(
				'record_id' => $context['results']['records'][$i]->nid,
				'algorithm_step' => $context['results']['records'][$i]->algorithm_step,
				'match_score' => $context['results']['records'][$i]->match_score,
			);
			if (isset($context['results']['records'][$i]->work_match)) {
				$record['recommended_work_id'] = $context['results']['records'][$i]->work_match->nid;
			}
			$is_rejected = _meta_link_work_check_rejected($record['record_id']);
			if (!$is_rejected) {
				drupal_write_record('meta_link_data', $record);
			}
			$context['sandbox']['progress']++;
			$context['message'] = t('Saving matches');
		}

		if ($context['sandbox']['progress'] < $context['sandbox']['max']) {
			$context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
		}
		else {
			$context['sandbox'] = array();
			$context['finished'] = 1;
		}
	}
}


/**
 * Helper function to check if the metadata record was rejected and shouldn't be matched anymore.
 *
 * @return TRUE | FALSE
 *		if this record was previously rejected, FALSE otherwise.
 */
function _meta_link_work_check_rejected($record_id) {
	$query = db_select('meta_link_data', 'meta');
	$query->condition('meta.record_id', $record_id)
			 ->fields('meta', array('record_id'));
	$res = $query->execute()->fetchAll();
	if(!empty($res)) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

/**
 * Helper function to save entity reference to the work.
 *
 * @param array matches_ids.
 *		array of the rows ids in the matches table.
 *
 */
function _meta_link_work_link_accept($matches_ids, $context) {
	$limit = 1;
	if (empty($context['sandbox'])) {
		$context['sandbox']['progress'] = 0;
		$context['sandbox']['current_record'] = 0;
		$context['sandbox']['max'] = count($matches_ids);
		$context['results']['matches'] = $matches_ids;
		$context['sandbox']['offset'] = 0;
	}
	else {
		$context['sandbox']['offset'] = $context['sandbox']['offset'] + $limit + 1;
	}
	$off = min($context['sandbox']['offset'] + $limit, $context['sandbox']['max'] - 1);
	for ($i = $context['sandbox']['offset']; $i <= $off; $i++) {
		$match = _meta_link_get_match_row($context['results']['matches'][$i]);
		if (isset($match) && isset($match->recommended_work_id) && !empty($match->recommended_work_id)) {
			$entity = node_load($match->record_id);
			$wrapper = entity_metadata_wrapper('node', $entity);
			$wrapper->field_work_reference->set($match->recommended_work_id);
			$wrapper->save();
			$context['results']['accepted_ids'][] = $context['results']['matches'][$i];
		}
		$context['sandbox']['progress']++;
		$context['message'] = t('Saving matches');
	}

	if ($context['sandbox']['progress'] < $context['sandbox']['max']) {
		$context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
	}
	else {
		$context['sandbox'] = array();
		$context['finished'] = 1;
	}
}


/**
 * Helper function to mark match as rejected.
 *
 * @param array matches_ids.
 *		array of the rows ids in the matches table.
 *
 */
function _meta_link_mark_as_rejected($matches_ids, $context) {
	$limit = 1;
	if (empty($context['sandbox'])) {
		$context['sandbox']['progress'] = 0;
		$context['sandbox']['current_record'] = 0;
		$context['sandbox']['max'] = count($matches_ids);
		$context['results']['matches'] = $matches_ids;
		$context['sandbox']['offset'] = 0;
	}
	else {
		$context['sandbox']['offset'] = $context['sandbox']['offset'] + $limit + 1;
	}
	$off = min($context['sandbox']['offset'] + $limit, $context['sandbox']['max'] - 1);
	for ($i = $context['sandbox']['offset']; $i <= $off; $i++) {
		_meta_link_set_reject_flag($context['results']['matches'][$i]);
		$context['sandbox']['progress']++;
		$context['message'] = t('Saving matches');
	}

	if ($context['sandbox']['progress'] < $context['sandbox']['max']) {
		$context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
	}
	else {
		$context['sandbox'] = array();
		$context['finished'] = 1;
	}
}

/**
 * Helper function to apply rejected flag to the match.
 *
 * @param int match_id
 *		id of the match row that needs to be updated.
 */
function _meta_link_set_reject_flag($match_id) {
	$query = db_update('meta_link_data')
				 ->fields(array('is_rejected' => 1))
				 ->condition('meta_link_data.id', $match_id)
				 ->execute();
}

/**
 * Helper function to delete accepted matches from the matches table.
 */
function _meta_link_delete_accepted_matches($context) {
	if (isset($context['results']['accepted_ids'])) {
		foreach ( $context['results']['accepted_ids'] as $key => $id) {
			$query = db_delete('meta_link_data')
				->condition('meta_link_data.id', $id)
				->execute();
		}
	}
}

/**
 * Helper function to get match record by id.
 *
 * @param int id
 *		id of a match row in matches table.
 *
 * @return object match
 *		db record.
 */
function _meta_link_get_match_row($id) {
	$query = db_select('meta_link_data', 'meta');
	$query->fields('meta', array('record_id', 'recommended_work_id'));
	$query->condition('meta.id', $id);
	$res = $query->execute()->fetchAll();
	if (isset($res[0])) {
		return $res[0];
	}
	else {
		return NULL;
	}
}

/**
 * Helper function to get all works.
 *
 * @return array res
 *		array of row objects that is returned by the query.
 */
function _meta_link_get_all_works() {
	$query = db_select('node', 'n');
	$query->fields('n', array('nid', 'title'));
	$query->condition('n.type', 'dll_work');
	$res = $query->execute()->fetchAll();
	return $res;
}



/**
 * Helper function with soundex query on the work title.
 *
 * @param str record_title
 *		title of the metadata record.
 *
 * @return array res
 *		array of match data | empty array if no match is found.
 */
function _meta_link_soundex_query($record_title) {
	$record_title = trim($record_title, '".,-');
	// we can't use
	$sql = "SELECT n.nid, n.title FROM {node} n WHERE soundex(n.title) LIKE CONCAT('%', soundex(:title_record), '%') AND n.type = 'dll_work'";
	$res = db_query($sql, array(':title_record' => $record_title))->fetchAll();
	// assuming the the first row has the best Levenshtein match score.
	$best_key = 0;
	$best_score = 1000;
	if (isset($res) && !empty($res)) {
		foreach ($res as $key => $row) {
			$score = levenshtein($row->title, $record_title);
			if ($score !== -1 && $score < $best_score) {
				$best_score = $score;
				$best_key = $key;
				$res[$key]->score = $score;
			}
		}
		return $res[$best_key];
	}
	else {
		return array();
	}
}

/**
 * Helper function with simple LIKE query on the work title.
 *
 * @param str record_title
 *		title of the metadata record.
 *
 * @return array res
 *		array of match data | empty array if no match is found.
 */
function _meta_link_primary_query($record_title) {
	$record_title_arr = explode("|", $record_title);
	if (!empty($record_title_arr)) {
		$query = db_select('node', 'n');
		$query->fields('n', array('nid', 'title'));
		$query->condition('n.type', 'dll_work');
		$title_or = db_or();
		foreach($record_title_arr as $title) {
			$title = trim($title, '".,-');
			$title_or->condition('n.title', '%' . db_like($title) . '%', 'LIKE');
		}
		$query->condition($title_or);
		$query->orderBy('n.title', 'ASC');
		$res = $query->execute()->fetchAll();
	}

	// assuming the the first row has the best Levenshtein match score.
	$best_key = 0;
	$best_score = 1000;
	if (isset($res) && !empty($res)) {
		foreach ($res as $key => $row) {
			foreach ($record_title_arr as  $rtitle) {
				$score = levenshtein($row->title, $rtitle);
				//dpm($score);
				if ($score !== -1 && $score < $best_score) {
					$best_score = $score;
					$best_key = $key;
					$res[$key]->score = $score;
				}
			}
		}
		return $res[$best_key];
	}
	else {
		return array();
	}
}
