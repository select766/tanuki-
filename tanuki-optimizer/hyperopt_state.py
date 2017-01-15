# pause/resume
class HyperoptState(object):
  def __init__(self):
    self.trials = Trials()
    self.iteration_logs = []

  def get_trials(self): return self.trials

  def get_n_accumulated_iterations(self): return len(self.iteration_logs)

  def record_iteration(self, **kwargs):
    # record any pickle-able object as a dictionary at each iteration.
    # also, len(self.iteration_logs) is used to keep the accumulated number of iterations.
    self.iteration_logs.append(kwargs)

  def calc_max_evals(self, n_additional_evals): return self.get_n_accumulated_iterations() + n_additional_evals

  @staticmethod
  def load(file_path):
    with open(file_path, 'rb') as fi:
      state = pickle.load(fi)
      print('resume from state: {} ({} iteration done)'.format(file_path, state.get_n_accumulated_iterations()))
      return state

  def save(self, file_path):
    try:
      with open(file_path, 'wb') as fo:
        pickle.dump(self, fo, protocol=-1)
        print('saved state to: {}'.format(file_path))
    except:
      print('failed to save state. continue.')

  def dump_log(self):
    for index, entry in enumerate(self.iteration_logs):
      # emulate function()'s output.
      print('-' * 78)
      print(datetime.datetime.today().strftime("%Y-%m-%d %H:%M:%S"))
      print(entry['args'])
      if index:
        remaining = datetime.timedelta(seconds=0)
        print(index, '/', self.get_n_accumulated_iterations(), str(remaining))
      popenargs = ['./YaneuraOu.exe',]
      print(popenargs)
      print(entry['output'])
      lose = entry['lose']
      draw = entry['draw']
      win = entry['win']
      ratio = 0.0
      if lose + draw + win > 0.1:
       ratio = win / (lose + draw + win)
      print ratio
