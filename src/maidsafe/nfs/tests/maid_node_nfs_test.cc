/*  Copyright 2014 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/nfs/tests/maid_node_nfs_test.h"

namespace maidsafe {

namespace nfs {

namespace test {


TEST_F(MaidNodeNfsTest, FUNC_Constructor) {
  auto maid_and_signer(passport::CreateMaidAndSigner());
  {
    auto nfs_new_account = nfs_client::MaidNodeNfs::MakeShared(maid_and_signer);
  }
  LOG(kInfo) << "joining existing account";
  auto nfs_existing_account = nfs_client::MaidNodeNfs::MakeShared(maid_and_signer.first);
}

TEST_F(MaidNodeNfsTest, FUNC_PutGet) {
  AddClient();
  ImmutableData data(NonEmptyString(RandomString(kTestChunkSize)));
  LOG(kVerbose) << "Before put";
  try {
    auto future(clients_.back()->Put(data));
    EXPECT_NO_THROW(future.get());
    LOG(kVerbose) << "After put";
  }
  catch (...) {
    GTEST_FAIL() << "Failed to put: " << DebugId(NodeId(data.name()->string()));
  }

  auto future(clients_.back()->Get<ImmutableData::Name>(data.name()));
  try {
    auto retrieved(future.get());
    EXPECT_EQ(retrieved.data(), data.data());
  }
  catch (...) {
    GTEST_FAIL() << "Failed to retrieve: " << DebugId(NodeId(data.name()->string()));
  }
}

TEST_F(MaidNodeNfsTest, FUNC_FailingGet) {
  ImmutableData data(NonEmptyString(RandomString(kTestChunkSize)));
  AddClient();
  auto future(clients_.back()->Get<ImmutableData::Name>(data.name()));
  EXPECT_THROW(future.get(), std::exception) << "must have failed";
}

TEST_F(MaidNodeNfsTest, FUNC_PutTimeout) {
  AddClient();
  ImmutableData data(NonEmptyString(RandomString(kTestChunkSize)));
  std::chrono::steady_clock::duration timeout(std::chrono::microseconds(1));
  auto future(clients_.back()->Put(data, timeout));

  try {
    future.get();
    EXPECT_TRUE(false) << "Unexpected put success: " << DebugId(NodeId(data.name()->string()));
  }
  catch (const maidsafe_error& error) {
    EXPECT_EQ(make_error_code(RoutingErrors::timed_out), error.code());
  }
}

TEST_F(MaidNodeNfsTest, FUNC_PutCancelled) {
  AddClient();
  ImmutableData data(NonEmptyString(RandomString(kTestChunkSize)));
  std::chrono::steady_clock::duration timeout(std::chrono::minutes(10));
  auto future(clients_.back()->Put(data, timeout));
  EXPECT_NO_THROW(clients_.back()->Stop());

  try {
    future.get();
    EXPECT_TRUE(false) << "Unexpected put success: " << DebugId(NodeId(data.name()->string()));
  }
  catch (const maidsafe_error& error) {
    EXPECT_EQ(make_error_code(RoutingErrors::timer_cancelled), error.code());
  }
}

TEST_F(MaidNodeNfsTest, FUNC_GetTimeout) {
  AddClient();
  ImmutableData data(NonEmptyString(RandomString(kTestChunkSize)));
  try {
    auto future(clients_.back()->Put(data));
    EXPECT_NO_THROW(future.get());
  }
  catch (...) {
    EXPECT_TRUE(false) << "Failed to put: " << DebugId(NodeId(data.name()->string()));
  }

  std::chrono::steady_clock::duration timeout(std::chrono::microseconds(1));
  auto future(clients_.back()->Get<ImmutableData::Name>(data.name(), timeout));

  try {
    auto retrieved(future.get());
    EXPECT_TRUE(false) << "Unexpected get success: " << DebugId(NodeId(data.name()->string()));
  }
  catch (const maidsafe_error& error) {
    EXPECT_EQ(make_error_code(RoutingErrors::timed_out), error.code());
  }
}

TEST_F(MaidNodeNfsTest, FUNC_GetCancelled) {
  AddClient();
  ImmutableData data(NonEmptyString(RandomString(kTestChunkSize)));
  try {
    auto future(clients_.back()->Put(data));
    EXPECT_NO_THROW(future.get());
  }
  catch (...) {
    EXPECT_TRUE(false) << "Failed to put: " << DebugId(NodeId(data.name()->string()));
  }

  std::chrono::steady_clock::duration timeout(std::chrono::minutes(10));
  auto future(clients_.back()->Get<ImmutableData::Name>(data.name(), timeout));
  EXPECT_NO_THROW(clients_.back()->Stop());

  try {
    auto retrieved(future.get());
    EXPECT_TRUE(false) << "Unexpected get success: " << DebugId(NodeId(data.name()->string()));
  }
  catch (const maidsafe_error& error) {
    EXPECT_EQ(make_error_code(RoutingErrors::timer_cancelled), error.code());
  }
}

TEST_F(MaidNodeNfsTest, FUNC_MultipleSequentialPuts) {
  routing::Parameters::caching = true;
  const size_t kIterations(10);
  GenerateChunks(kIterations);
  AddClient();
  int index(0);
  for (const auto& chunk : chunks_) {
    auto future(clients_.back()->Put(chunk));
    EXPECT_NO_THROW(future.get()) << "Store failure " << DebugId(NodeId(chunk.name()->string()));
    LOG(kVerbose) << DebugId(NodeId(chunk.name()->string())) << " stored: " << index++;
  }

  std::vector<boost::future<ImmutableData>> get_futures;
  for (const auto& chunk : chunks_) {
    get_futures.emplace_back(clients_.back()->Get<ImmutableData::Name>(
        chunk.name(), std::chrono::seconds(kIterations * 2)));
  }
  CompareGetResult(chunks_, get_futures);
  LOG(kVerbose) << "Multiple sequential puts is finished successfully";
}

TEST_F(MaidNodeNfsTest, FUNC_MultipleParallelPuts) {
  routing::Parameters::caching = false;
  LOG(kVerbose) << "put 10 chunks with 1 clients";
  PutGetTest(1, 10);
  LOG(kVerbose) << "Multiple parallel puts test has finished successfully";
}

TEST_F(MaidNodeNfsTest, FUNC_MultipleClientsPut) {
  LOG(kVerbose) << "put 10 chunks with 5 clients";
  PutGetTest(5, 10);
  LOG(kVerbose) << "Put with multiple clients test has finished successfully";
}

TEST_F(MaidNodeNfsTest, FUNC_DataFlooding) {
  LOG(kVerbose) << "flooding 100 chunks with 10 clients";
  PutGetTest(10, 100);
  LOG(kVerbose) << "Data flooding test has finished successfully";
}

/*
// The test below is disbaled as its proper operation assumes a delete funcion is in place
TEST_F(MaidNodeNfsTest, DISABLED_FUNC_PutMultipleCopies) {
  ImmutableData data(NonEmptyString(RandomString(kTestChunkSize)));
  boost::future<ImmutableData> future;
  GetClients().front()->Put(data);
  Sleep(std::chrono::seconds(2));

  GetClients().back()->Put(data);
  Sleep(std::chrono::seconds(2));

  {
    future = GetClients().front()->Get<ImmutableData::Name>(data.name());
    try {
      auto retrieved(future.get());
      EXPECT_EQ(retrieved.data(), data.data());
    }
    catch (...) {
      GTEST_FAIL() << "Failed to retrieve: " << DebugId(NodeId(data.name()->string()));
    }
  }
  {
    future = GetClients().back()->Get<ImmutableData::Name>(data.name());
    try {
      auto retrieved(future.get());
      EXPECT_EQ(retrieved.data(), data.data());
    }
    catch (...) {
      GTEST_FAIL() << "Failed to retrieve: " << DebugId(NodeId(data.name()->string()));
    }
  }
  GetClients().front()->Delete<ImmutableData::Name>(data.name());
  Sleep(std::chrono::seconds(2));
  try {
    auto retrieved(env_->Get<ImmutableData>(data.name()));
    EXPECT_EQ(retrieved.data(), data.data());
  }
  catch (...) {
    GTEST_FAIL() << "Failed to retrieve: " << DebugId(NodeId(data.name()->string()));
  }
  GetClients().back()->Delete<ImmutableData::Name>(data.name());
  Sleep(std::chrono::seconds(2));

  routing::Parameters::caching = false;
  EXPECT_THROW(env_->Get<ImmutableData>(data.name()), std::exception)
      << "Should have failed to retreive";
  routing::Parameters::caching = true;
}
*/

TEST_F(MaidNodeNfsTest, FUNC_PopulateSingleBranchTree) {
  ImmutableData chunk(NonEmptyString(RandomAlphaNumericString(1024)));
  const size_t max_versions(5), max_branches(1);
  GenerateChunks(max_versions * 2);
  StructuredDataVersions::VersionName v_ori(0, chunks_.front().name());
  AddClient();
  auto create_version_future(clients_.back()->CreateVersionTree(chunk.name(), v_ori,
      static_cast<uint32_t>(max_versions), static_cast<uint32_t>(max_branches)));
  EXPECT_NO_THROW(create_version_future.get()) << "failure to create version";
  for (size_t index(1); index < (max_versions * 2); ++index) {
    StructuredDataVersions::VersionName v_old(index - 1, chunks_[index - 1].name());
    StructuredDataVersions::VersionName v_new(index, chunks_[index].name());
    auto put_version_future(clients_.back()->PutVersion(chunk.name(), v_old, v_new));
    EXPECT_NO_THROW(put_version_future.get()) << "failure to put version " << index;
  }
  try {
    auto future(clients_.back()->GetVersions(chunk.name()));
    auto versions(future.get());
    EXPECT_EQ(versions.size(), max_branches);
    EXPECT_EQ(versions.front().index, max_versions * 2 - 1);
    EXPECT_EQ(versions.front().id, chunks_.back().name());
  } catch (const maidsafe_error& error) {
    GTEST_FAIL() << "Failed to retrieve version: " << boost::diagnostic_information(error);
  }

  try {
    StructuredDataVersions::VersionName v_tip(max_versions * 2 - 1, chunks_.back().name());
    auto future(clients_.back()->GetBranch(chunk.name(), v_tip));
    auto versions(future.get());
    EXPECT_EQ(versions.size(), max_versions);
    for (size_t index(0); index < versions.size(); ++index) {
      EXPECT_EQ(versions[index].index, max_versions * 2 - index - 1);
      EXPECT_EQ(versions[index].id, chunks_[max_versions * 2 - index - 1].name());
    }
  } catch (const maidsafe_error& error) {
    GTEST_FAIL() << "Failed to retrieve branch: " << boost::diagnostic_information(error);
  }
}

TEST_F(MaidNodeNfsTest, FUNC_PopulateMultipleBranchTree) {
  VersionTreeTest(5, 4, 20);
  VersionTreeTest(100, 10, 60);
}

}  // namespace test

}  // namespace nfs

}  // namespace maidsafe
