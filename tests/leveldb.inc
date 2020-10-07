<?php

function leveldb_empty($db) {
	$it = new LevelDBIterator($db);

	for($it->rewind(); $it->valid(); $it->next()) {
		$db->delete($it->key());
	}
}
