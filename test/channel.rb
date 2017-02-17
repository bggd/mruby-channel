assert 'Channel.get' do

  assert_kind_of Channel, Channel.get('foo')

end

assert 'Channel#recv' do

  chan = Channel.get 'foo'

  chan.send 1

  ok = chan.recv { |v|
    assert_equal 1, v
  }

  assert_true ok

end

assert 'Routine#initialize' do

  assert_kind_of Routine, Routine.new('x = 1')

end

assert 'Routine with Channel' do

  chan = Channel.get 'foo'

  thread = Routine.new("
    done = Channel.get 'done'
    chan = Channel.get 'foo'
    chan.send([1, 'a', :b, true, nil])
    chan.send({ :abc => 123 })
    chan.close
  ")

  thread.start

  ok = chan.recv { |v|
    assert_kind_of Array, v
    assert_equal [1, 'a', :b, true, nil], v
  }
  assert_true ok

  ok = chan.recv { |v|
    assert_kind_of Hash, v
    assert_equal({ :abc => 123 }, v)
  }
  assert_true ok

  ok = chan.recv { |v| }
  assert_false ok

end

assert 'Routine#join' do

  sender = Channel.get '1'
  receiver = Channel.get '2'

  thread = Routine.new("
    sender = Channel.get '2'
    receiver = Channel.get '1'

    sender.send 1
    receiver.recv { |v| }
  ")

  assert_false thread.running?
  thread.start

  receiver.recv { |v| }

  assert_true thread.running?

  sender.send 'done'

  thread.join

  assert_false thread.running?
end

assert 'Channel#try_recv' do

  chan = Channel.get 'try'

  thread = Routine.new("
    chan = Channel.get 'try'
    chan.send 0.1
  ")

  10.times do |i|
    assert_false chan.try_recv { |v| }
  end

  thread.start
  thread.join

  assert_true chan.try_recv { |v| assert_kind_of Float, v }

end

assert 'unknown type' do

  class Unk; end

  chan = Channel.get 'unk'

  assert_raise(TypeError) { chan.send Unk.new }

end

assert 'Routine#get_error' do

  th = Routine.new 'puts 1'

  th.start
  th.join
  assert_equal nil, th.get_error

  th = Routine.new 'puts x'

  th.start
  th.join
  assert_kind_of String, th.get_error

end
