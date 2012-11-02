<?php

function cleanup_leveldb_on_shutdown() {
	register_shutdown_function('cleanup_leveldb');
}

function cleanup_leveldb() {
	global $leveldb_path, $db;
	if (file_exists($leveldb_path)) {
		if($db instanceof LevelDB) {
			$db->close();
		}
		LevelDB::destroy($leveldb_path);
	}
}

function leveldb_empty($db) {
	$it = new LevelDbIterator($db);

	for($it->rewind(); $it->valid(); $it->next()) {
		$db->delete($it->key());
	}
}
