#include <iostream>
#include <fstream>

#include <json\json.h>
#include <json\json-forwards.h>
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <glm\glm.hpp>

struct BoneAnimation {
	std::vector<aiQuatKey> rotationKeys;
	std::vector<aiVectorKey> positionKeys;
	std::vector<aiVectorKey> scaleKeys;
};

struct MeshBone {
	std::vector<float> weights;
	std::map<std::string, BoneAnimation> animations;
};

struct Mesh {
	std::string name;
	std::vector<aiVector3D> vertex;
	std::vector<aiVector3D> uv;
	std::vector<unsigned short> index;
	std::string diffuseMap;
	std::string normalMap;
	//std::vector<glm::vec2> vertexWeights;
	std::map< std::string, MeshBone > bones;

	std::string error;
};

void pause() {
	std::cout << "\n\n";
	system("PAUSE");
}

Json::Value meshToJM(Mesh mesh) {
	Json::Value root;

	root["name"] = mesh.name;
	root["material"]["diffuse"] = mesh.diffuseMap;
	root["material"]["normal"] = mesh.normalMap;

	int numVertices = mesh.vertex.size();
	for (unsigned int i = 0; i < numVertices; ++i) {
		Json::Value v;
		v["x"] = mesh.vertex[i].x;
		v["y"] = mesh.vertex[i].y;
		v["z"] = mesh.vertex[i].z;
		root["vertices"].append(v);
	};

	int numUVs = mesh.uv.size();
	for (unsigned int i = 0; i < numUVs; ++i) {
		Json::Value v;
		v["x"] = mesh.uv[i].x;
		v["y"] = mesh.uv[i].y;
		root["uvs"].append(v);
	};

	int numIndices = mesh.index.size();
	for (unsigned int i = 0; i < numIndices; ++i) {
		root["indices"].append(mesh.index[i]);
	};

	Json::Value bones;

	//typedef std::map< std::string, std::map<std::string, std::vector<aiQuatKey>> >::iterator outerQuat;
	//typedef std::map< std::string, std::map<std::string, std::vector<aiVectorKey>> >::iterator outerVector;
	//typedef std::map<std::string, std::vector<aiQuatKey>>::iterator innerQuat;
	//typedef std::map<std::string, std::vector<aiVectorKey>>::iterator innerVector;

	typedef std::map< std::string, MeshBone>::iterator MeshBoneIterator;
	typedef std::map< std::string, BoneAnimation>::iterator BoneAnimationIterator;
	//typedef std::map< std::string, aiQuatKey>::iterator RotKeyIterator;
	//typedef std::map< std::string, aiVectorKey>::iterator PosKeyIterator;
	//typedef std::map< std::string, aiVectorKey>::iterator SclKeyIterator;

	for (MeshBoneIterator i = mesh.bones.begin(); i != mesh.bones.end(); ++i) {
		std::string boneName = i->first;
		MeshBone bone = i->second;

		for (unsigned int j = 0; j < bone.weights.size(); ++j) {
			Json::Value weight = bone.weights[j];
			bones[boneName]["weights"].append(weight.asFloat());
		}

		for (BoneAnimationIterator j = bone.animations.begin(); j != bone.animations.end(); ++j) {

			Json::Value animation;

			std::string animationName = j->first;
			BoneAnimation boneAnimation = j->second;

			for(unsigned int k = 0; k < boneAnimation.rotationKeys.size(); ++k) {
				aiQuatKey key = boneAnimation.rotationKeys[k];

				Json::Value values;
				values["x"] = key.mValue.x;
				values["y"] = key.mValue.y;
				values["z"] = key.mValue.z;

				int frame = key.mTime;
				animation[animationName][frame]["rotationKeys"] = values;
			}

			bones[boneName]["animations"] = animation;
		}
	}

	root["bones"] = bones;

	return root;
};

bool writeJsonValueToFile(std::string filepath, Json::Value json) {

	Json::StyledWriter writer;
	//Json::FastWriter writer;

	//std::cout << writer.write(json); 

	std::ofstream file;
	file.open(filepath);
	file << writer.write(json);
	file.close();

	return true;
}

Mesh populateMeshFromDae(std::string filePath) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filePath, NULL);

	Mesh mesh;

	if (scene == NULL) {
		mesh.error = importer.GetErrorString();
		return mesh;
	}

	//aiNode* rootNode = scene->mNumMeshes;

	std::cout << "\nNormal: " << scene->mNumMeshes;
	std::cout << "\nRoot: " << scene->mRootNode->mNumMeshes;

	int hasMesh = scene->mNumMeshes;

	//hasMesh = 0;
	if (!hasMesh) {
		mesh.error = "No meshes found in scene.";
		return mesh;
	}

	aiMesh* m = scene->mMeshes[0];
	mesh.name = m->mName.C_Str();

	//std::cout << "\nRoot: " << m->;

	int numVertices = m->mNumVertices;
	for (unsigned int i = 0; i < numVertices; ++i) {
		mesh.vertex.push_back(m->mVertices[i]);
	}

	//int numUVComponents = m->mNumUVComponents;
	for (unsigned int i = 0; m->mNumUVComponents[i] > 0; ++i) {
		for (unsigned int j = 0; j < numVertices; ++j) {
			mesh.uv.push_back(m->mTextureCoords[i][j]);
		}
	}

	int numFaces = m->mNumFaces;
	for (unsigned int i = 0; i < numFaces; ++i) {
		for (unsigned int j = 0; j < m->mFaces[i].mNumIndices; ++j) {
			mesh.index.push_back(m->mFaces[i].mIndices[j]); 
		}
	}

	if (m->mMaterialIndex > -1) {
		std::cout << "\nMaterials found."; 

		int materialIndex = m->mMaterialIndex;
		aiMaterial* material = scene->mMaterials[materialIndex];

		aiTextureType type;

		type = aiTextureType(aiTextureType_DIFFUSE);
		int diffuseTextureCount = material->GetTextureCount(type);
		if (diffuseTextureCount > 0) {
			for (unsigned int i = 0; i < diffuseTextureCount; ++i) {
				aiString path;
				material->GetTexture(type, i, &path);
				mesh.diffuseMap = path.C_Str();
			}
		}

		type = aiTextureType(aiTextureType_NORMALS);
		int normalTextureCount = material->GetTextureCount(type);
		if (normalTextureCount > 0) {
			for (unsigned int i = 0; i < normalTextureCount; ++i) {
				aiString path;
				material->GetTexture(type, i, &path);
				mesh.normalMap = path.C_Str();
			}
		}

		//std::cout << "\n\nNumTex: " << ;
	} else { 
		std::cout << "\nNo materials found."; 
	}

	if (m->HasBones()) {

		std::cout << "\n\nBones found; loading bones and animations.";

		std::map<std::string, int> bones;
		for (unsigned int i = 0; i < m->mNumBones; ++i) {
			aiBone* bone = m->mBones[i];
			std::string boneName = bone->mName.C_Str();
			std::cout << "\nBone name: " << boneName;
			bones[boneName] = i;

			int numWeights = bone->mNumWeights;
			mesh.bones[boneName].weights.resize(numWeights);
			for (unsigned int j = 0; j < numWeights; ++j) {
				aiVertexWeight weight = bone->mWeights[j];
				mesh.bones[boneName].weights[weight.mVertexId] = weight.mWeight;
			};
		};

		int numAnimations = scene->mNumAnimations;
		for (unsigned int i = 0; i < numAnimations; ++i) {

			aiAnimation* animation = scene->mAnimations[i];
			std::string animationName = animation->mName.C_Str();
			std::cout << "\nAnimation name: " << animationName;
			int numChannels = animation->mNumChannels;
			for (unsigned int j = 0; i < numChannels; ++i) {

				aiNodeAnim* animationChannel = animation->mChannels[j];
				std::string nodeName = animationChannel->mNodeName.C_Str();

				if (bones[nodeName] > -1) {

					std::string boneName = nodeName;
					BoneAnimation* boneAnimation = &mesh.bones[boneName].animations[animationName];

					for (int k = 0; k < scene->mAnimations[0]->mChannels[bones[nodeName]]->mNumRotationKeys; ++k) {
						boneAnimation->rotationKeys.push_back(animationChannel->mRotationKeys[k]);
					};

					for (int k = 0; k < scene->mAnimations[0]->mChannels[bones[nodeName]]->mNumPositionKeys; ++k) {
						boneAnimation->positionKeys.push_back(animationChannel->mPositionKeys[k]);
					};

					for (int k = 0; k < scene->mAnimations[0]->mChannels[bones[nodeName]]->mNumScalingKeys; ++k) {
						boneAnimation->scaleKeys.push_back(animationChannel->mScalingKeys[k]);
					};

					//Animations* animations = &mesh.animations;
					//if (animations->position

				}
			};
		};

		// Load bone weights.

	} else {
		std::cout << "\nNo bones/armature found.";
	}

	return mesh;
};

int main () {

	std::cout << "\n****\nThis utility only loads one mesh and all it's animations from the dae scene.\n****\n";

	//Mesh mesh = populateMeshFromDae("box.dae");
	//Mesh mesh = populateMeshFromDae("animBox.dae");
	//Mesh mesh = populateMeshFromDae("animBox.fbx");
	//Mesh mesh = populateMeshFromDae("animBox.3ds");
	Mesh mesh = populateMeshFromDae("animBox.x");
	if (mesh.error.length() > 0) {
		std::cout << "\n\nError: " << mesh.error;
		pause();
		return 1;
	}

	Json::Value jm = meshToJM(mesh);
	writeJsonValueToFile("JSON.jm", jm);

	/*
	std::cout << root;


	*/

	pause();
	return 0;
};