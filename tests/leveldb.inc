<?php

function cleanup_leveldb_on_shutdown() {
	register_shutdown_function('cleanup_leveldb');
}

function cleanup_leveldb() {
	global $leveldb_path;
	if (file_exists($leveldb_path)) {
		LevelDB::destroy($leveldb_path);
	}
}

function leveldb_empty($db) {
	$it = new LevelDbIterator($db);

	for($it->rewind(); $it->valid(); $it->next()) {
		$db->delete($it->key());
	}
}
