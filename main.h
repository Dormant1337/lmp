#ifndef MAIN_H
#define MAIN_H

typedef struct Track {
	char	name[50];
	char	path[256];
} Track;

typedef struct Playlist {
	char	name[50];
	int	track_indices[100];
	int	track_count;
} Playlist;

typedef struct AppState {
	Track	 library[100];
	Playlist playlists[20];
	int	 playlist_count;
	int	 track_count;
	int	 is_running;
	int	 current_volume;
	char	 current_track[256];
	char	 command_buffer[256];
	char	 message[2048]; 
	double	 track_duration;
	int	 playing_playlist_index;
	int	 playing_track_index_in_playlist;
} AppState;

#endif /* MAIN_H */