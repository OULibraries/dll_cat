<?php
/**
 * @file
 * Helper functions for Archive Meta Import module.
 */

/**
 * Helper function to get XML from the archive repository by URL.
 *
 * @param str url
 *		url to the OAI-PMH endpoint.
 *
 * @return str data | FALSE
 *		xml markup string or FALSE if no data is awailable.
 */
function _archive_meta_import_get_xml($url) {
	$ch = curl_init();
	curl_setopt($ch, CURLOPT_HEADER, 0);
	// Set curl to return the data instead of printing it to the browser.
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
	curl_setopt($ch, CURLOPT_URL, $url);
	$data = curl_exec($ch);
	if (!curl_errno($ch)) {
		$info = curl_getinfo($ch,  CURLINFO_HTTP_CODE);
		if ($info!= 200) {
			$data = FALSE;
		}
	}
	else {
		$data = FALSE;
	}
	curl_close($ch);
	return $data;
}


/**
 * Helper function to parse XML markup string.
 *
 * @param str data
 *		XML markup string data.
 *
 * @return array parsed_data
 *		associative array of parsed XML data.
 */
function _archive_meta_import_parse_xml($data) {
	$parsed_data = array();
	if(!$data) {
		return $parsed_data;
	}
	$xml = simplexml_load_string($data);
	if (isset($xml)) {
		$xml_bc = clone($xml);
		$request_repo = qp($xml_bc, 'request')->text();
		$records = qp($xml, 'ListRecords')->find('record');
		if (isset($records)) {
			// getting metadata spec
			$schema = _archive_meta_import_define_schema();
			foreach ($records as $key => $record) {
				$record_bc = clone($record);
				$meta = $record->find('metadata')->xml();
				$identifier = $record_bc->find('header > identifier')->text();

				// TODO account for different metadata prefixes
				// stripping out namespace prefix as we don't need it anymore
				$meta_bc = str_replace('dc:', '', $meta);
				$meta_bc = preg_replace('/xsi:schemaLocation=\"([^"]+)\"/', '', $meta_bc);
				if (isset($schema) && !empty($schema)) {
					foreach ($schema as $property) {
						$meta = qp(simplexml_load_string($meta_bc), 'metadata');
						$text = '';
						$text = $meta->find($property);
						if (isset($text)) {
							foreach ($text as $t) {
								$t = $t->text();
								if (!empty($t)) {
									$parsed_data[$key]['metadata'][$property][] = $t;
								}
							}
						}
					}
				}
				if (!empty($parsed_data) && isset($identifier) && !empty($identifier)) {
					$parsed_data[$key]['unique_identifier'] = $identifier;
					if (isset($request_repo) && !empty($request_repo)) {
						$parsed_data[$key]['repo'] = $request_repo;
					}
					else {
						$parsed_data[$key]['repo'] = '';
					}
				}
			}
		}
		//drush_log(var_dump($parsed_data), 'success');
	}
	return $parsed_data;
}

/**
 * Helper function to prepare and save metadata node.
 *
 * @param array data
 *		associative array of metadata.
 */
function _archive_meta_import_prep_meta_node($data, $identifier, $repo) {
	if (!isset($data)) {
		// this record was deleted, we shouldn't process it.
		return;
	}
	global $user;
	$default_params = array(
		'type' => 'metadata',
		'uid' => $user->uid,
		'status' => 1,
		'comment' => 0,
		'promote' => 0,
	);
	$entity = entity_create('node', $default_params);
	$wrapper = entity_metadata_wrapper('node', $entity);
	$nid = $wrapper->getIdentifier();

	// setting title
	if (isset($data['title'][0]) && !empty($data['title'][0])) {
		$title = $data['title'][0];
		$sub_t = substr($title, 0, 240) . '_' . time();

		if ($sub_t) {
			$wrapper->title->set($sub_t);
		}
		else {
			$wrapper->title->set($title);
		}
	}
	else {
		$wrapper->title->set('Metadata - ' . $nid);
	}

	_archive_meta_import_set_field_values($data, $identifier, $wrapper, $entity, $repo);
	$wrapper->save(TRUE);
}

/**
 * Helper function to update metadata node.
 *
 * @param int nid
 *		node id of the node to be updated.
 *
 * @param array $data
 *		associative array of metadata.
 *
 * @param str identifier
 *		unique identifier of the recprd provided by the repo.
 */
function _archive_meta_import_update_meta_node($nid, $data, $identifier, $repo) {
	$entity = node_load($nid);
	$wrapper = entity_metadata_wrapper('node', $entity);
	if (!isset($data)) {
		$wrapper->status = 0;
	}
	else {
		_archive_meta_import_set_field_values($data, $identifier, $wrapper, $entity, $repo);
	}
	$wrapper->save(TRUE);
}


/**
 * Helper funciton to check if record already exists in the db.
 *
 * @param str identifier
 *		unique identifier of the record.
 */
function _archive_meta_import_check_node_by_identifier($identifier) {
	if (!$identifier || empty($identifier)) {
		return FALSE;
	}
	$query = db_select('node', 'n');
	$query->fields('n', array('nid'));
	$query->join('field_data_field_unique_identifier', 'id', 'id.entity_id = n.nid');
	$query->condition('id.field_unique_identifier_value', $identifier);
	$res = $query->execute()->fetchAll();
	if (isset($res[0]->nid)) {
		return $res[0]->nid;
	}
	else {
		return FALSE;
	}
}


/**
 * Helper function to update/fill in field values for the node.
 *
 * @param array data
 * 		associative array of metadata.
 *
 * @param str identifier
 *		unique identifier of the record provided by the repository
 *
 * @param Entity API object wrapper
 *		metadata wrapper.
 *
 * @param Entity API object entity
 *		node entity.
 */
function _archive_meta_import_set_field_values($data, $identifier, $wrapper, $entity, $repo) {
	// setting Record title field (not a node title field).
	$wrapper->field_record_title = array();
	foreach ($data['title'] as $title) {
		$wrapper->field_record_title[] = $title;
	}

	// setting unique identifier
	if (isset($identifier)) {
		$wrapper->field_unique_identifier->set($identifier);
	}

	// setting source repo
	if (isset($repo) && $repo !== '') {
		$wrapper->field_repository_source->set($repo);
	}

	// setting description
	if (isset($data['description'])) {
		foreach ($data['description'] as $value) {
			if (isset($value) && !empty($value)) {
				$wrapper->body->set(array('value' => $value));
			}
		}
	}

	// getting properties
	$schema = _archive_meta_import_define_schema();
	foreach ($schema as $property) {
		if ($property !== 'title' && $property !== 'description') {
			$field_name = 'field_' . $property;
			$wrapper->{$field_name} = array();
			if (isset($data[$property])) {
				foreach ($data[$property] as $value) {
					if (isset($value) && !empty($value)) {
						$wrapper->{$field_name}[] = $value;
					}
				}
			}
		}
	}

	// setting date
	/*if (isset($data['date'])) {
		foreach ($data['date'] as $k => $d) {
			$date = new DateTime($d);
			$entity->field_date[LANGUAGE_NONE][$k] = array(
				'value' => date_format($date, 'Y-m-d'),
				'timzone' => 'UTC',
				'timezone_db' => 'UTC',
			);
		}
	}*/
}
/**
 * Helper function to define metadata schema.
 *
 * @param str schema_prefix
 *		metadata schema identifier.
 *
 * @return array $schema_namespaces
 *		array of namespaces that will be used as selectors for the parser.
 */
function _archive_meta_import_define_schema($schema_prefix = 'oai_dc') {
	$schema_namespaces = array(
		'title',
		'creator',
		'subject',
		'description',
		'publisher',
		'contributor',
		'date',
		'type',
		'format',
		'identifier',
		'source',
		'language',
		'relation',
		'coverage',
		'rights',
	);
	return $schema_namespaces;
}

/**
 * Helper function to populate urls of repositories queue.
 */
function _archive_meta_import_urls_queue_populate() {
	$queue = DrupalQueue::get('archive_meta_import_process_urls');
	if(!$queue->numberOfItems() || $queue->numberOfItems() == 0) {
  	$urls = variable_get('meta_repo_urls');
		if (isset($urls) && !empty($urls)) {
			foreach ($urls as $key => $url) {
				if ($url['url'] !== '') {
					$queue->createItem($url['url']);
				}
			}
		}
  }
}

/**
 * Helper function to populate metadata queue.
 *
 * @param str data
 *		XML string.
 */
function _archive_meta_import_metadata_queue_populate($data, $item) {
	$queue = DrupalQueue::get('archive_meta_import_metadata');
	$i = array('url' => $item, 'data' => $data,);
	$queue->createItem($i);
}

/**
 * Helper function to populate records queue.
 *
 * @param array data
 *		associative array of parsed record.
 */
function _archive_meta_import_records_queue_populate($data) {
	$queue = DrupalQueue::get('archive_meta_import_records');
	$queue->createItem($data);
}