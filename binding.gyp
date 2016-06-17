{
  'targets': [
    {
      'target_name': '<(module_name)',
      'sources': [
        'src/scale.cc',
        'src/crop.cc',
        'src/buffersize.cc',
        'src/compress.cc',
        'src/decompress.cc',
        'src/exports.cc',
        'src/tools.cc'
      ],
      'include_dirs': [
        '<!(node -e "require(\'nan\')")'
      ],
      'dependencies': [
        'deps/libjpeg-turbo.gyp:jpeg-turbo'
      ]
    },
    {
      'target_name': 'action_after_build',
      'type': 'none',
      'dependencies': [ '<(module_name)' ],
      'copies': [
        {
          'files': [ '<(PRODUCT_DIR)/<(module_name).node' ],
          'destination': '<(module_path)'
        }
      ]
    }
  ]
}
