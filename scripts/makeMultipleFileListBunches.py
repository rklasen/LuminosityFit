#! /bin/python

import os, sys, re, errno, glob, time
import subprocess
import multiprocessing

cpu_cores = multiprocessing.cpu_count()

lib_path = os.path.abspath('argparse-1.2.1/build/lib')
sys.path.append(lib_path)

import argparse

dirs = []
pattern = ''

def getListOfDirectories(path):
  if os.path.isdir(path):
    for dir in os.listdir(path):
      dirpath = path + '/' + dir
      if os.path.isdir(dirpath):
        getListOfDirectories(dirpath)
      else:
        match = re.search('Lumi_TrksQA_\d*\.root', dir)
        if match:
          match_dir_pattern = re.search(pattern, path)
          if match_dir_pattern:
            dirs.append(path)
          return

filename_prefix = 'Lumi_TrksQA_'

def createFileListFile(output_url, list_of_files):
    f = open(output_url, 'w')
    for file_url in list_of_files:
        f.write(file_url)
        f.write('\n')
    f.close()

def makeFileListBunches(directory):
    good_files = []
    bad_files = []
    found_files = glob.glob(directory + '/' + filename_prefix + '*')
    for file in found_files:
        if os.stat(file).st_size > 20000:
            good_files.append(file)
        else:
            bad_files.append(file)
    
    if len(good_files) < len(bad_files):
        print 'WARNING: found more bad than good files... ' + str(len(good_files)) + ' good files vs ' + str(len(bad_files)) + ' bad files! Something went wrong here...'
    
    print 'detected ' + str(len(bad_files)) + ' bad files'
    print 'creating file lists...'
    
    max_bundles = len(good_files) / args.files_per_bunch
    if len(good_files) % args.files_per_bunch > 0:
        max_bundles += 1
    output_bunch_dir = directory + '/bunches_' + str(max_bundles)
    
    try:
      os.makedirs(output_bunch_dir)
    except OSError as exception:
      if exception.errno != errno.EEXIST:
        print 'error: thought dir does not exists but it does...'
    
    file_list_index = 1
    while len(good_files) > 0:
        # get next chunk of good files
        chunk_size = args.files_per_bunch
        if len(good_files) < args.files_per_bunch:
            chunk_size = len(good_files)
        
        chunk_of_good_files = []
        for i in range(1, chunk_size + 1):
            chunk_of_good_files.append(good_files.pop())
        
        createFileListFile(output_bunch_dir + '/filelist_' + str(file_list_index) + '.txt', chunk_of_good_files)
        file_list_index += 1
    

parser = argparse.ArgumentParser(description='Script for going through whole directory trees and generating filelist bunches for faster lmd data creation.', formatter_class=argparse.RawTextHelpFormatter)

parser.add_argument('dirname', metavar='dirname_to_scan', type=str, nargs=1,
                    help='Name of directory to scan recursively for qa files and create bunches')
parser.add_argument('--files_per_bunch', metavar='files_per_bunch', type=int, default=4, help='number of root files used for a bunch')
parser.add_argument('--directory_pattern', metavar='directory_pattern', type=str, default='.*', help='Only directories with according to this pattern will be used.')

args = parser.parse_args()

pattern = args.directory_pattern

getListOfDirectories(args.dirname[0])

for dir in dirs:
  makeFileListBunches(dir)