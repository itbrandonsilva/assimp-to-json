#include <iostream>
#include <fstream>

#include <json\json.h>
#include <json\json-forwards.h>
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <glm\glm.hpp>

struct AnimationKeys {
	std::vector<aiQuatKey> rotationKeys;
	std::vector<aiVectorKey> positionKeys;
	std::vector<aiVectorKey> scaleKeys;
};

struct MeshBone {
	int index;
	int pindex;
	aiMatrix4x4 offsetMatrix;
	aiMatrix4x4 nodeTransform;
	std::string parentName;
	std::map<int, float> weights;
	std::map<std::string, AnimationKeys> animations;
};

struct AnimationInfo {
	float length;
	float fps;
};

struct Mesh {
	std::string name;
	int numFaces;
	std::vector<aiVector3D> vertex;
	std::vector<aiVector3D> normal;
	std::vector<aiVector3D> uv;
	std::vector<unsigned short> index;
	std::string diffuseMap;
	std::string normalMap;
	std::map< std::string, MeshBone > bones;
	std::map< std::string, AnimationInfo > animations;

	std::string error;
};

struct VertexBoneWeights {
	std::vector<int> boneIds;
	std::vector<float> boneWeights;
};

typedef std::map< std::string, MeshBone >::iterator MeshBonesIterator;
typedef std::map< std::string, AnimationInfo >::iterator AnimationInfoIterator;
typedef std::map< std::string, AnimationKeys >::iterator AnimationKeysIterator;
typedef std::map< int, VertexBoneWeights >::iterator VertexBoneWeightsIterator;

void pause() {
	std::cout << "\n\n";
	system("PAUSE");
}

Json::Value meshToJM(Mesh mesh) {
	Json::Value root;
	std::cout << "\n\nBuilding JSON.";

	Json::Value vertices = Json::Value(Json::arrayValue);
	int numVertices = mesh.vertex.size();
	for (unsigned int i = 0; i < numVertices; ++i) {
		vertices.append(mesh.vertex[i].x);
		vertices.append(mesh.vertex[i].y);
		vertices.append(mesh.vertex[i].z);
	};
	root["vertices"] = vertices;

	Json::Value uvs = Json::Value(Json::arrayValue);
	int numUVs = mesh.uv.size();
	for (unsigned int i = 0; i < numUVs; ++i) {
		uvs.append(mesh.uv[i].x);
		uvs.append(mesh.uv[i].y);
	};
	root["uvs"].append(uvs);

	Json::Value faces = Json::Value(Json::arrayValue);
	int numIndices = mesh.index.size();
	for (unsigned int i = 0; i < numIndices; i+=3) {
		faces.append(10);
		faces.append(mesh.index[i]);
		faces.append(mesh.index[i+1]);
		faces.append(mesh.index[i+2]);
		faces.append(0);
		faces.append(i);
		faces.append(i+1);
		faces.append(i+2);
	};
	root["faces"] = faces;

	Json::Value normals = Json::Value(Json::arrayValue);
	int numNormals = mesh.normal.size();
	for (unsigned int i = 0; i < numNormals; ++i) {
		normals.append(mesh.normal[i].x);
		normals.append(mesh.normal[i].y);
		normals.append(mesh.normal[i].z);
	};
	root["normals"] = normals;

	Json::Value metadata;
	metadata["formatVersion"] = 3.1f;
    metadata["formatVersion"].asFloat();
	metadata["generatedBy"] = "assimp-to-json converter";
	metadata["vertices"] = numVertices;
	metadata["faces"] = mesh.numFaces;
	metadata["description"] = "void.";
	root["metadata"] = metadata;

	Json::Value materials = Json::Value(Json::arrayValue);

	Json::Value material;
	material["DbgColor"] = 15658734;
	material["DbgIndex"] = 0;
	material["DbgName"] = "cube_mat";
	material["mapDiffuse"] = mesh.diffuseMap;
	//material["mapNormal"] = mesh.normalMap;
	materials.append(material);
	root["materials"] = materials;

	Json::Value bones          = Json::Value(Json::arrayValue);
	Json::Value skinIndices    = Json::Value(Json::arrayValue);
	Json::Value skinWeights    = Json::Value(Json::arrayValue);

	Json::Value animation;
	for (AnimationInfoIterator it = mesh.animations.begin(); it != mesh.animations.end(); ++it) {
		AnimationInfo info = it->second;
		animation["name"] = it->first;
		animation["length"] = info.length;
		animation["fps"] = info.fps;
		animation["JIT"] = 0;
		animation["hierarchy"] = Json::Value(Json::arrayValue);
	}

	for (MeshBonesIterator i = mesh.bones.begin(); i != mesh.bones.end(); ++i) {
		Json::Value jsonBone;
		MeshBone bone = i->second;
		std::string boneName = i->first;
		jsonBone["name"] = boneName;
		jsonBone["parent"] = bone.pindex;

		aiMatrix4x4 om = bone.nodeTransform;
		aiVector3D dscl; aiQuaternion drot; aiVector3D dpos;
		om.Decompose(dscl, drot, dpos);

		Json::Value pos = Json::Value(Json::arrayValue);
		pos.append(dpos.x); pos.append(dpos.y); pos.append(dpos.z);
		Json::Value rotq = Json::Value(Json::arrayValue);
		rotq.append(drot.x); rotq.append(drot.y); rotq.append(drot.z); rotq.append(drot.w);
		Json::Value scl = Json::Value(Json::arrayValue);
		scl.append(dscl.x); scl.append(dscl.y); scl.append(dscl.z);

		jsonBone["pos"] = pos;
		jsonBone["scl"] = scl;
		jsonBone["rotq"] = rotq;

		/* 
		
		Some example JSON models include the "rot" field on bones, thought they don't seem to be required
		when "rotq" is populated.

		Json::Value rot = Json::Value(Json::arrayValue);
		rot.append(1); rot.append(1); rot.append(1);
		jsonBone["rot"] = rot;

		*/

		bones[bone.index] = jsonBone;

		std::map<int, VertexBoneWeights> vertexWeights;

		/* 
		
		The next two loops map vertex indices to weight information relevant to that vertex.

		The first loop organizes/maps the bone weight information to each vertex.
		The second loop maps the data into the Threejs JSON format which will be exported.

		Right now, I'm only processing two bones per vertex until I confirm the Threejs importer
		can handle more.

		*/

		typedef std::map<int, float>::iterator it_type;
		for(it_type it = bone.weights.begin(); it != bone.weights.end(); it++) {

			int vertexId = it->first;
			VertexBoneWeights* w = &vertexWeights[vertexId];
			if (w->boneIds.size() > 2) {
				continue;
			}

			w->boneIds.push_back(bone.index);
			w->boneWeights.push_back(it->second);
		}

		for( VertexBoneWeightsIterator it = vertexWeights.begin(); it != vertexWeights.end(); ++it ) {
			
			int vertexId = it->first;
			VertexBoneWeights w = it->second;

			int boneId = w.boneIds[0];
			float boneWeight = w.boneWeights[0];

			// Update to check if boneIds are populated and append data accordingly.
			skinIndices[vertexId+vertexId] = boneId; skinIndices[vertexId+vertexId+1] = 0;
			skinWeights[vertexId+vertexId] = boneWeight; skinWeights[vertexId+vertexId+1] = 0;

		}

		root["skinWeights"] = skinWeights;
		root["skinIndices"] = skinIndices;

		for (AnimationInfoIterator j = mesh.animations.begin(); j != mesh.animations.end(); ++j) {

			std::string animationName = j->first;
			AnimationKeys animationKeys = bone.animations[animationName];

			Json::Value hierarchyBone;
			hierarchyBone["parent"] = bone.pindex;
			hierarchyBone["keys"] = Json::Value(Json::arrayValue);

			for (unsigned int l = 0; l < animationKeys.rotationKeys.size(); ++l) {

				Json::Value key;

				aiQuatKey rotationKey = animationKeys.rotationKeys[l];
				key["time"] = rotationKey.mTime;
				key["rot"] = Json::Value(Json::arrayValue);
				key["rot"].append(rotationKey.mValue.x);
				key["rot"].append(rotationKey.mValue.y);
				key["rot"].append(rotationKey.mValue.z);
				key["rot"].append(rotationKey.mValue.w);

				aiVectorKey positionKey = animationKeys.positionKeys[l];
				key["pos"] = Json::Value(Json::arrayValue);
				key["pos"].append(positionKey.mValue.x);
				key["pos"].append(positionKey.mValue.y);
				key["pos"].append(positionKey.mValue.z);

				aiVectorKey scaleKey = animationKeys.scaleKeys[l];
				key["scl"] = Json::Value(Json::arrayValue);
				key["scl"].append(scaleKey.mValue.x);
				key["scl"].append(scaleKey.mValue.y);
				key["scl"].append(scaleKey.mValue.z);

				hierarchyBone["keys"].append(key);
			}

			animation["hierarchy"].append(hierarchyBone);

		}

	}

	root["animation"] = animation;
	root["bones"] = bones;

	std::cout << "\nDone building JSON.";
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

	int numMeshes = scene->mNumMeshes;
	if (!numMeshes) {
		mesh.error = "No meshes found in scene.";
		return mesh;
	}

	if (numMeshes > 1) {
		mesh.error = "Please export only 1 mesh.";
		return mesh;
	}

	aiMesh* m = scene->mMeshes[0];
	mesh.name = m->mName.C_Str();

	bool hasNormals = m->HasNormals();
	int numVertices = m->mNumVertices;

	if (hasNormals) {
		std::cout << "\nVertex normals found.";
	} else {
		std::cout << "\nPrecalculated normals from source not found.";
	}

	std::cout << "\nNum Vertices: " << numVertices;
	for (unsigned int i = 0; i < numVertices; ++i) {
		mesh.vertex.push_back(m->mVertices[i]);
		if (hasNormals) {
			mesh.normal.push_back(m->mNormals[i].Normalize());
		}
	}

	for (unsigned int i = 0; m->mNumUVComponents[i] > 0; ++i) {
		for (unsigned int j = 0; j < numVertices; ++j) {
			mesh.uv.push_back(m->mTextureCoords[i][j]);
		}
	}

	int numFaces = m->mNumFaces;
	std::cout << "\nNum Faces: " << numFaces;
	mesh.numFaces = numFaces;
	for (unsigned int i = 0; i < numFaces; ++i) {
		for (unsigned int j = 0; j < m->mFaces[i].mNumIndices; ++j) {
			mesh.index.push_back(m->mFaces[i].mIndices[j]); 
		}
	}

	int materialIndex = m->mMaterialIndex;
	if (materialIndex >= 0) {
		std::cout << "\n\nMaterials found."; 

		int materialIndex = m->mMaterialIndex;
		aiMaterial* material = scene->mMaterials[materialIndex];

		aiTextureType type;

		type = aiTextureType(aiTextureType_DIFFUSE);
		int diffuseTextureCount = material->GetTextureCount(type);
		if (diffuseTextureCount > 0) {
			std::cout << "\n    Diffuse map found: ";
			for (unsigned int i = 0; i < diffuseTextureCount; ++i) {
				aiString path;
				material->GetTexture(type, i, &path);
				std::cout << path.C_Str();
				mesh.diffuseMap = path.C_Str();
			}
		} else {
			std::cout << "\n    No diffuse map found.";
		}

		/* Until I can actually get normal maps working properly in Three.js, I'll leave this unimplemented.
		type = aiTextureType(aiTextureType_NORMALS);
		int normalTextureCount = material->GetTextureCount(type);
		if (normalTextureCount > 0) {
			std::cout << "\n    Normal map found: ";
			for (unsigned int i = 0; i < normalTextureCount; ++i) {
				aiString path;
				material->GetTexture(type, i, &path);
				std::cout << path.C_Str();
				mesh.normalMap = path.C_Str();
			}
		} else {
			std::cout << "\n    No normal map found.";
		}

		*/
	} else { 
		std::cout << "\n\nNo materials found."; 
	}

	aiMatrix4x4 armatureTransformation;
	aiMatrix4x4 sceneTransformation = scene->mRootNode->mTransformation;

	if (m->HasBones()) {
		std::cout << "\n\nBones found; loading bones.";

		// We shouldn't assume the armature name is "Armature" in the future.
	    const char* armatureName = "Armature";
		aiNode* armature = scene->mRootNode->FindNode(armatureName);

		armatureTransformation = armature->mTransformation;

		std::cout << "\nNum Bones: " << m->mNumBones;

		std::vector<aiMatrix4x4> boneMatrices(m->mNumBones);
		for (unsigned int i = 0; i < m->mNumBones; ++i) {
			aiBone* bone = m->mBones[i];
			std::string boneName = bone->mName.C_Str();

			aiNode* boneNode = armature->FindNode(boneName.c_str());
			aiNode* parentNode = boneNode->mParent;

			boneMatrices[i] = bone->mOffsetMatrix;
			const aiNode* tempNode = boneNode;
			while (tempNode) {
				boneMatrices[i] *= tempNode->mTransformation;
				tempNode = tempNode->mParent;
			}

			mesh.bones[boneName].index = i;
			mesh.bones[boneName].parentName = parentNode->mName.C_Str();
			mesh.bones[boneName].offsetMatrix = boneMatrices[i];
			mesh.bones[boneName].nodeTransform = boneNode->mTransformation;

			aiMatrix4x4 omm;
			aiQuaternion drott; aiVector3D dposs;
			omm = mesh.bones[boneName].offsetMatrix;
			omm.DecomposeNoScaling(drott, dposs);

			int numWeights = bone->mNumWeights;
			std::cout << "\n    Bone (name): " << boneName;
			std::cout << "\n        Num Influenced vertices: " << numWeights;

			for (unsigned int j = 0; j < numWeights; ++j) {
				aiVertexWeight weight = bone->mWeights[j];
				mesh.bones[boneName].weights[weight.mVertexId] = weight.mWeight; 
			};
		};

		// Populate the pindex property of each bone with the bone index of the bones parent.
		for (MeshBonesIterator i = mesh.bones.begin(); i != mesh.bones.end(); ++i) {
			MeshBone* meshBone = &i->second;
			if (meshBone->parentName == armatureName) {
				meshBone->pindex = -1;
			} else {
				meshBone->pindex = mesh.bones[meshBone->parentName].index;
			}
		}

		int numAnimations = scene->mNumAnimations;
		if (numAnimations > 0) {
			std::cout << "\n\nAnimations found; loading animations.";
		}

		for (unsigned int i = 0; i < numAnimations; ++i) {
			aiAnimation* animation = scene->mAnimations[i];

			std::string animationName = animation->mName.C_Str();
			std::cout << "\n    Animation name: \"" << animationName << "\"";

			AnimationInfo animationInfo;
			animationInfo.length = animation->mDuration;
			animationInfo.fps    = animation->mTicksPerSecond;
			std::cout << "\n        Frame rate: " << animation->mTicksPerSecond;
			mesh.animations[animationName] = animationInfo;

			int numChannels = animation->mNumChannels;
			for (unsigned int j = 0; j < numChannels; ++j) {

				aiNodeAnim* animationChannel = animation->mChannels[j];
				std::string boneName = animationChannel->mNodeName.C_Str();
				int boneIndex = mesh.bones[boneName].index;

				AnimationKeys* animationKeys = &mesh.bones[boneName].animations[animationName];
				for (int k = 0; k < scene->mAnimations[i]->mChannels[boneIndex]->mNumRotationKeys; ++k) {
					animationKeys->rotationKeys.push_back(animationChannel->mRotationKeys[k]);
				};

				for (int k = 0; k < scene->mAnimations[i]->mChannels[boneIndex]->mNumPositionKeys; ++k) {
					animationKeys->positionKeys.push_back(animationChannel->mPositionKeys[k]);
				};

				for (int k = 0; k < scene->mAnimations[i]->mChannels[boneIndex]->mNumScalingKeys; ++k) {
					animationKeys->scaleKeys.push_back(animationChannel->mScalingKeys[k]);
				};

			};
		};

	} else {
		std::cout << "\nNo bones found.";
	}

	return mesh;
};

int main (int argc, char* argv[]) {

	std::cout << "\n****";
	std::cout << "\nThis utility converts one mesh at a time."; 
	std::cout << "\nBe sure that your source, whatever format it may be, contains one mesh.";
	std::cout << "\n  Supports:";
	std::cout << "\n    - Texture/diffuse map";
	std::cout << "\n    - Skinned animations";
	std::cout << "\n****\n";

	if (argc < 2) {
		std::cout << "\nPlease specify a file to convert. Supported formats are determined by assimp.";
		pause();
		return 1;
	}
	std::string filename = argv[1];
	filename = "animBoxNewer.dae";

	Mesh mesh = populateMeshFromDae(filename);
	if (mesh.error.length() > 0) {
		std::cout << "\n\nError: " << mesh.error;
		pause();
		return 1;
	}

	Json::Value jm = meshToJM(mesh);

	// Update so that the filename resembles the original filename.
	writeJsonValueToFile("JSON.js", jm);

	pause();
	return 0;
};