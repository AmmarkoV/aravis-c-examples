project('aravis-c-examples', 'c', version: '0.0.1')

aravis_dep = dependency('aravis-0.10')

examples = [
  '01-single-acquisition',
  '02-multiple-acquisition-main-thread',
  '02-multiple-acquisition-callback',
  '02-multiple-acquisition-signal',
  '03-camera-api',
  '04-camera-features',
  '05-chunk-parser',
  '06-grabber',
  '07-streamer'
]
 
lib_dir = meson.current_source_dir()
shared_lib = meson.get_compiler('c').find_library('SharedMemoryVideoBuffers', dirs : lib_dir, required: true)

foreach e: examples
  if e == '07-streamer'
    if shared_lib.found()
      exe = executable(e, e + '.c', dependencies: [aravis_dep, shared_lib])
    else
      message('Skipping 07-streamer: SharedMemoryVideoBuffers library not found.')
    endif
  else
    exe = executable(e, e + '.c', dependencies: aravis_dep)
  endif
endforeach
