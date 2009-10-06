/*
    Copyright (C) 2009 Andrew Caudwell (acaudwell@gmail.com)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version
    3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dirnode.h"

float gGourceMinDirSize   = 15.0;
float gGourceMaxSpeed     = 1000.0f;

float gGourceForceGravity = 10.0;
float gGourceDirPadding   = 1.5;

float gGourceElasticity   = 0.0;

bool  gGourceNodeDebug    = false;
bool  gGourceGravity      = true;
bool  gGourceDrawDirName  = true;

//debugging
int  gGourceDirNodeInnerLoops = 0;
int  gGourceFileInnerLoops = 0;

std::map<std::string, RDirNode*> gGourceDirMap;

TextureResource* beamtex = 0;

RDirNode::RDirNode(RDirNode* parent, std::string abspath) {

    changePath(abspath);

    parent = 0;
    setParent(parent);

    //figure out starting position
    if(parent !=0) {
        vec2f parentPos = parent->getPos();
        vec2f offset;

        RDirNode* parentP = parent->getParent();

        pos = parentPos;
    } else {
        pos = vec2f(0.0f, 0.0f);
    }

    if(beamtex==0) {
        beamtex = texturemanager.grab("beam.png");
    }

    float padded_file_radius  = gGourceFileDiameter * 0.5;

    file_area  = padded_file_radius * padded_file_radius * PI;

    visible_count = 0;

    calcRadius();
    calcColour();

    visible = false;
    position_initialized = false;

    since_node_visible = 0.0;
    since_last_file_change = 0.0;
    since_last_node_change = 0.0;
}

void RDirNode::changePath(std::string abspath) {
    //fix up path
    if(abspath.size() == 0 || abspath[abspath.size()-1] != '/') {
        abspath = abspath + std::string("/");
    }

    gGourceDirMap.erase(this->abspath);

    //debugLog("new dirnode %s\n", abspath.c_str());

    this->abspath = abspath;

    gGourceDirMap[abspath] = this;
}

RDirNode::~RDirNode() {
    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        delete (*it);
    }

    gGourceDirMap.erase(abspath);
}

int RDirNode::getTokenOffset() {
    return path_token_offset;
}

void RDirNode::fileUpdated(bool userInitiated) {
    calcRadius();

    if(userInitiated) since_last_file_change = 0.0;

    nodeUpdated(userInitiated);
}

void RDirNode::nodeUpdated(bool userInitiated) {
    if(userInitiated) since_last_node_change = 0.0;

    calcRadius();
    updateFilePositions();

    if(parent !=0) parent->nodeUpdated(true);
}

void RDirNode::setPos(vec2f pos) {
    this->pos = pos;
}

//returns true if supplied path prefixes the nodes path
bool RDirNode::prefixedBy(std::string path) {
    if(path.size()==0) return false;

    if(path[path.size()-1] != '/') {
        path = path + std::string("/");
    }

    int pos = abspath.find(path);

    if(pos==0) return true;

    return false;
}

std::string RDirNode::getPath() {
    return abspath;
}



RDirNode* RDirNode::getParent() {
    return parent;
}

int RDirNode::getDepth() {
    return depth;
}

void RDirNode::adjustPath() {

    //update display path
    int parent_token_offset = 0;

    path_token_offset = abspath.size();

    if(parent != 0) {
        parent_token_offset  = parent->getTokenOffset();

        //debugLog("abspath.substr arguments: %d %d %s size = %d\n", parent_token_offset, abspath.size()-parent_token_offset-1, abspath.c_str(), abspath.size());

        path_token        = abspath.substr(parent_token_offset, abspath.size()-parent_token_offset-1);
        path_token_offset = abspath.size();

        //debugLog("new token %s\n", path_token.c_str());
    }
}

void RDirNode::setParent(RDirNode* parent) {
    if(parent != 0 && this->parent == parent) return;

    this->parent = parent;

    adjustPath();

    //set depth
    if(parent == 0) {
        depth = 1.0;
    } else {
        depth = parent->getDepth() + 1;
    }
}


void RDirNode::addNode(RDirNode* node) {
    // does this node prefix any other nodes, if so, add them to it

    std::vector<RDirNode*> matches;
    std::string path = node->getPath();

    //debugLog("adding node %s to %s\n", path.c_str(), abspath.c_str());

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); ) {
        RDirNode* child = (*it);

        if(child->prefixedBy(path)) {
            it = children.erase(it);
            node->addNode(child);
        } else {
            it++;
        }
    }

    // add to this node
    children.push_back(node);
    node->setParent(this);

    //debugLog("added node %s to %s\n", node->getPath().c_str(), getPath().c_str());

    nodeUpdated(false);
}

RDirNode* RDirNode::getRoot() {
    if(parent==0) return this;
    return parent->getRoot();
}

// note - you still need to delete the file yourself
bool RDirNode::removeFile(RFile* f) {
    //doesnt match this path at all
    if(f->path.find(abspath) != 0) {
        return false;
    }

    //is this dir - add to this node
    if(f->path.compare(abspath) == 0) {

        for(std::list<RFile*>::iterator it = files.begin(); it != files.end(); it++) {
            if((*it)==f) {
                files.erase(it);
                if(!f->isHidden()) visible_count--;

                fileUpdated(false);

                return true;
            }
        }

        return false;
    }

    //does this belong to one of the children ?
    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);
        bool removed = node->removeFile(f);

        if(removed) {
            //node is now empty, reap!
            if(node->fileCount()==0 && node->dirCount()==0) {
                children.erase(it);
                debugLog("deleting node %s...\n", node->getPath().c_str());
                delete node;
                nodeUpdated(false);
            }

            return true;
        }
    }

    return false;
}

void RDirNode::addVisible() {
    visible_count++;
    visible = true;
}

bool RDirNode::isVisible() {

    if(visible) return true;

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        if((*it)->isVisible()) {
            visible = true;
            return true;
        }
    }

    return false;
}

int RDirNode::visibleFileCount() {
    return visible_count;
}

int RDirNode::fileCount() {
    return files.size();
}

std::string RDirNode::commonPathPrefix(std::string& str) {
    int c = 0;
    int slash = -1;

    while(c<abspath.size() && c<str.size() && abspath[c] == str[c]) {
        if(abspath[c] == '/') {
            slash = c;
        }
        c++;
    }

    if(slash==-1) return "";
    return str.substr(0,slash+1);
}

bool RDirNode::addFile(RFile* f) {

    //doesnt match this path at all
    if(f->path.find(abspath) != 0) {

        if(parent!=0) return false;

        RDirNode* newparent;

        std::string common = commonPathPrefix(f->path);
        if(common.size()==0) common = "/";

        newparent = new RDirNode(0, common);
        newparent->addNode(this);
        return newparent->addFile(f);
    }

    //simply change path of node and add this to it
    if(   parent==0 && abspath == "/"
       && f->path.compare(abspath) != 0 && fileCount()==0 && dirCount()==0) {
        debugLog("modifying root path to %s\n", f->path.c_str());
        changePath(f->path);
    }

    //is this dir - add to this node
    if(f->path.compare(abspath) == 0) {
        //debugLog("addFile %s to %s\n", f->fullpath.c_str(), abspath.c_str());

        files.push_back(f);
        if(!f->isHidden()) visible_count++;
        f->setDir(this);

        fileUpdated(false);

        return true;
    }

    //does this belong to one of the children ?
    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* child =  (*it);

        bool added = child->addFile(f);

        if(added) return true;
    }

    //add new child, add it to that
    //if commonpath is longer than abspath, add intermediate node, else just add at the files path
    RDirNode* node = new RDirNode(this, f->path);

    node->addFile(f);

    addNode(node);

    // do we have dir nodes, with a common path element greater than abspath,
    // if so create another node, and move those nodes there

     std::string commonpath;
     vec2f commonPos;
     for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
         RDirNode* child =  (*it);

         std::string common = child->commonPathPrefix(f->path);
         if(common.size() > abspath.size() && common != f->path) {
            commonpath = common;
            commonPos = child->getPos();
            break;
         }
     }

    // redistribute to new common node
    if(commonpath.size() > abspath.size()) {
        //debugLog("common path %s\n", commonpath.c_str());

        RDirNode* cnode = new RDirNode(this, commonpath);
        cnode->setPos(commonPos);

        for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end();) {
            RDirNode* child =  (*it);

            if(child->prefixedBy(commonpath)) {
                //debugLog("this path = %s, commonpath = %s, path = %s\n", abspath.c_str(), commonpath.c_str(), child->getPath().c_str());
                it = children.erase(it);
                cnode->addNode(child);
                continue;
            }

            it++;
        }

        addNode(cnode);
    }

    return true;
}

float RDirNode::getRadius() {
    return dir_radius;
}

float RDirNode::getRadiusSqrt() {
    return dir_radius_sqrt;
}

vec3f RDirNode::averageFileColour() {

    vec3f av;
    int count = 0;

    for(std::list<RFile*>::iterator it = files.begin(); it != files.end(); it++) {
        RFile* file = (*it);

        if(file->isHidden()) continue;

        av += file->getColour();

        count++;
    }

    if(count>0) av *= (1.0f/(float)count);

    count = 0;

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end();it++) {
            RDirNode* child =  (*it);

            av += child->averageFileColour();
            count++;
    }

    if(count>0) av *= (1.0f/(float)count);

    return av;
}

vec4f RDirNode::getColour() {
    return col;
}

void RDirNode::calcColour() {

    // make branch brighter if recently accessed
    float brightness = std::max(0.6f, 1.0f - std::min(1.0f, since_last_node_change / 3.0f));

    col = vec4f(brightness, brightness, brightness, 1.0);

    int fcount = 0;

    for(std::list<RFile*>::iterator it = files.begin(); it != files.end(); it++) {
        RFile* file = (*it);

        if(file->isHidden()) continue;;

        vec3f filecol = file->getColour() * brightness;
        float a       = file->getAlpha();

        col += vec4f(filecol.x, filecol.y, filecol.z, a);

        fcount++;
    }

    this->col /= (float) fcount + 1.0;
}

void RDirNode::calcArea() {

    float total_file_area = file_area * visible_count;

    dir_area = total_file_area;

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);

        dir_area += node->getArea();
    }
}

float RDirNode::getArea() {
    return dir_area;
}

void RDirNode::calcRadius() {
    calcArea();

    // was gGourceMinDirSize
    this->dir_radius = std::max(1.0f, (float)sqrt(dir_area)) * gGourceDirPadding;
    this->dir_radius_sqrt = sqrt(dir_radius);
}

float RDirNode::distanceTo(RDirNode* node) {

    float posd        = (node->getPos() - pos).length();
    float myradius    = getRadius();

    float your_radius = std::max<float>(1.0f, sqrt((static_cast<float>(node->visibleFileCount()) * file_area)) * gGourceDirPadding);

    float sumradius = myradius + your_radius;

    float distance = posd - sumradius;

    return distance;
}

void RDirNode::applyForceDir(RDirNode* node) {
    if(node == this) return;

    vec2f node_pos = node->getPos();

    vec2f dir = node_pos - pos;

    float posd2       = dir.length2();
    float myradius    = getRadius();
    float your_radius = node->getRadius();

    float sumradius = (myradius + your_radius);

    float distance2 = posd2 - sumradius*sumradius;

    if(distance2>0.0) return;

    float posd = sqrt(posd2);

    float distance = posd - myradius - your_radius;

    //resolve overlap
    if(posd < 0.00001) {
        accel += vec2f( (rand() % 100) - 50, (rand() % 100) - 50).normal();
        return;
    }

    accel += distance * dir.normal();
}

vec2f RDirNode::getPos() {
    return pos;
}

bool RDirNode::isParentOf(RDirNode* node) {

    if(node->prefixedBy(abspath)) return true;

    return false;
}

bool RDirNode::empty() {
    return (visible_count==0 && dirCount()==0) ? true : false;
}

void RDirNode::applyForces(QuadTree& quadtree) {

    //child nodes
    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);

        node->applyForces(quadtree);
    }

    if(parent == 0) return;

    std::vector<QuadItem*> inbounds;
    int found = quadtree.getItemsInBounds(inbounds, quadItemBounds);

    std::set<std::string> seen;
    std::set<std::string>::iterator seentest;

    //apply forces with other that are inside the 'box' of this nodes radius
    for(std::vector<QuadItem*>::iterator it = inbounds.begin(); it != inbounds.end(); it++) {

        RDirNode* d = (RDirNode*) (*it);

        if(d==this) continue;
        if(d==parent) continue;
        if(d->parent==this) continue;

        if((seentest = seen.find(d->getPath())) != seen.end()) {
            continue;
        }

        seen.insert(d->getPath());

        if(isParentOf(d)) continue;
        if(d->isParentOf(this)) continue;

        applyForceDir(d);

        gGourceDirNodeInnerLoops++;
    }

    //always call on parent no matter how far away
    applyForceDir(parent);

    //pull towards parent
    float parent_dist = distanceTo(parent);

    //  * dirs should attract to sit on the radius of the parent dir ie:
    //    should attract to distance_to_parent * normal_to_parent

    accel += gGourceForceGravity * parent_dist * (parent->getPos() - pos).normal();

    //  * dirs should be pushed along the parent_parent to parent normal by a force smaller than the parent radius force
    RDirNode* pparent = parent->getParent();

    if(pparent != 0) {
        vec2f parent_edge = (parent->getPos() - pparent->getPos());
        vec2f parent_edge_normal = parent_edge.normal();

        vec2f dest = (parent->getPos() + (parent->getRadius() + getRadius()) * parent_edge_normal) - pos;

        accel += dest;
    }

    //  * dirs should repulse from other dirs of this parent
    std::list<RDirNode*>* siblings = parent->getChildren();
    if(siblings->size() > 0) {
        vec2f sib_accel;

        int visible = 1;

        for(std::list<RDirNode*>::iterator it = siblings->begin(); it != siblings->end(); it++) {
            RDirNode* node = (*it);

            if(node == this) continue;
            if(!node->isVisible()) continue;

            visible++;

            sib_accel -= (node->getPos() - pos).normal();
        }

        //parent circumfrence divided by the number of visible child nodes
        if(visible>1) {
            float slice_size = (parent->getRadius() * PI) / (float) (visible+1);
            sib_accel *= slice_size;

            accel += sib_accel;
        }
    }

}

void RDirNode::debug(int indent) {
    std::string indentstr;
    while(indentstr.size() < indent) indentstr += " ";

    debugLog("%s%s\n", indentstr.c_str(), abspath.c_str());

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);
        node->debug(indent+1);
    }
}

int RDirNode::totalFileCount() {
    int total = visibleFileCount();

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);
        total += node->visibleFileCount();
    }

    return total;
}

int RDirNode::totalDirCount() {
    int total = 1;

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);
        total += node->totalDirCount();
    }

    return total;
}

int RDirNode::dirCount() {
    return children.size();
}

std::list<RDirNode*>* RDirNode::getChildren() {
    return &children;
}

void RDirNode::updateSpline(float dt) {
    if(parent == 0) return;

    //update the spline point
    vec2f td = (parent->getPos() - pos) * 0.5;

    vec2f mid = pos + td;// - td.perpendicular() * pos.normal();// * 10.0;

    vec2f delta = (mid - spos);

    //dont let spos get more than half the length of the distance behind
    if(delta.length2() > td.length2()) {
        spos += delta.normal() * (delta.length() - td.length());
    }

    spos += delta * std::min(1.0, dt * 2.0);
}

void RDirNode::setInitialPosition() {
    RDirNode* parentP = parent->getParent();

    pos = parent->getPos();

    //offset position by some pseudo-randomness
    if(parentP != 0) {
        pos += ((parent->getPos() - parentP->getPos()).normal() * 2.0 + vec2Hash(abspath)).normal();
    }  else {
        pos += vec2Hash(abspath);
    }

    //the spline point
    spos = pos - (parent->getPos() - pos) * 0.5;
    position_initialized=true;
}

void RDirNode::move(float dt) {

    //the root node is the centre of the world
    if(parent == 0) {
        pos = vec2f(0.0, 0.0);
        return;
    }

    //initial position
    if(!empty() && !position_initialized) {
        setInitialPosition();
    }

    pos += accel * dt;

    if(gGourceElasticity>0.0) {
        vec2f diff = (accel - prev_accel);

        float m = dt * gGourceElasticity;

        vec2f accel3 = prev_accel * (1.0-m) + diff * m;
        pos += accel3;
        prev_accel = accel3;
    }

    //accel = accel * std::max(0.0f, (1.0f - dt*10.0f));
    accel = vec2f(0.0, 0.0);
}

vec2f RDirNode::getNodeNormal() {
    return node_normal;
}

vec2f RDirNode::calcFileDest(int max_files, int file_no) {

    float arc   = 1.0/(float)max_files;

    float frac = arc * 0.5 + arc * file_no;

    vec2f dest = vec2f(sinf(frac*PI*2.0), cosf(frac*PI*2.0));

    return dest;
}

void RDirNode::updateFilePositions() {

    int max_files = 1;
    int diameter  = 1;
    int file_no   = 0;

    int files_left = visible_count;

    for(std::list<RFile*>::iterator it = files.begin(); it!=files.end(); it++) {
        RFile* f = *it;

        if(f->isHidden()) {
            f->setDest(vec2f(0.0,0.0));
            f->setDistance(0.0f);
            continue;
        }

        vec2f dest = calcFileDest(max_files, file_no);

        float d = ((float)(diameter-1)) * gGourceFileDiameter;

        f->setDest(dest);
        f->setDistance(d);

        files_left--;
        file_no++;

        if(file_no>=max_files) {
            diameter++;
            max_files = (int) std::max(1.0, diameter*PI);

            if(files_left<max_files) {
                max_files = files_left;
            }

            file_no=0;
        }
    }
}

void RDirNode::calcEdges() {

    calcProjectedPos();

    //calculate edges
    splines.clear();

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* child = *it;

        child->calcEdges();

        splines[child] = SplineEdge(projected_pos, col, child->getProjectedPos(), child->getColour(), child->getSPos());
    }
}

void RDirNode::logic(float dt, Bounds2D& bounds) {

    //move
    move(dt);
    updateSpline(dt);

    //update node normal
    if(parent != 0) {
        node_normal = (pos - parent->getPos()).normal();
    }

    bounds.update(pos);

    //update files
     for(std::list<RFile*>::iterator it = files.begin(); it!=files.end(); it++) {
         RFile* f = *it;

         f->logic(dt);
     }

    //update child nodes
    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);

        node->logic(dt, bounds);
    }

    //update colour
    calcColour();

    //update tickers
    if(visible) since_node_visible += dt;

    since_last_file_change += dt;
    since_last_node_change += dt;
}

void RDirNode::drawDirName(FXFont& dirfont) {
    if(!gGourceDrawDirName) return;

    if(since_last_node_change > 5.0) return;

    float alpha = std::max(0.0f, 5.0f - since_last_node_change) / 5.0f;

    glColor4f(1.0, 1.0, 1.0, alpha);

    vec3f screenpos = display.project(vec3f(0.0, 0.0, 0.0));

    glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
            glOrtho(0, display.width, display.height, 0, -1.0, 1.0);

        glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

        dirfont.draw(screenpos.x, screenpos.y, path_token);

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
}

void RDirNode::drawNames(FXFont& dirfont, Frustum& frustum) {

    glPushMatrix();
    glTranslatef(pos.x, pos.y, 0.0);

    if(isVisible()) {
        drawDirName(dirfont);
    }

    if(frustum.boundsInFrustum(quadItemBounds)) {
        for(std::list<RFile*>::iterator it = files.begin(); it!=files.end(); it++) {
            RFile* f = *it;
            f->drawName();
        }
    }

    glPopMatrix();


    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);
        node->drawNames(dirfont,frustum);
    }
}

void RDirNode::drawBeam(vec2f pos_src, vec4f col_src, vec2f pos_dest, vec4f col_dest, float beam_radius) {

    vec2f perp = (pos_src - pos_dest).perpendicular().normal();

    // src point
    glColor4fv(col_src);
    glTexCoord2f(0.0,0.0);
    glVertex2f(pos_src.x - perp.x * beam_radius, pos_src.y - perp.y * beam_radius);
    glTexCoord2f(1.0,0.0);
    glVertex2f(pos_src.x + perp.x * beam_radius, pos_src.y + perp.y * beam_radius);

    // dest point
    glColor4fv(col_dest);
    glTexCoord2f(1.0,0.0);
    glVertex2f(pos_dest.x + perp.x * beam_radius, pos_dest.y + perp.y * beam_radius);
    glTexCoord2f(0.0,0.0);
    glVertex2f(pos_dest.x - perp.x * beam_radius, pos_dest.y - perp.y * beam_radius);
}

void RDirNode::drawShadows(Frustum &frustum, float dt) {

    if(frustum.boundsInFrustum(quadItemBounds)) {

        glPushMatrix();
        glTranslatef(pos.x, pos.y, 0.0);

        //draw files
        for(std::list<RFile*>::iterator it = files.begin(); it!=files.end(); it++) {
            RFile* f = *it;
            if(f->isHidden()) continue;

            f->drawShadow(dt);
        }

        glPopMatrix();
    }

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);
        node->drawShadows(frustum, dt);
    }
}

void RDirNode::drawFiles(Frustum &frustum, float dt) {

    if(frustum.boundsInFrustum(quadItemBounds)) {

        vec4f col = getColour();

        glPushMatrix();
            glTranslatef(pos.x, pos.y, 0.0);

            //draw files

            for(std::list<RFile*>::iterator it = files.begin(); it!=files.end(); it++) {
                RFile* f = *it;
                if(f->isHidden()) continue;

                f->draw(dt);
            }

        glPopMatrix();
    }

    glDisable(GL_TEXTURE_2D);

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);
        node->drawFiles(frustum,dt);
    }
}

vec2f RDirNode::getSPos() {
    return projected_spos;
}

vec2f RDirNode::getProjectedPos() {
    return projected_pos;
}

void RDirNode::calcProjectedPos() {
    projected_pos  = display.project(vec3f(pos.x, pos.y, 0.0)).truncate();
    projected_spos = display.project(vec3f(spos.x, spos.y, 0.0)).truncate();
}

void RDirNode::drawEdgeShadows(float dt) {

    if(parent==0) glBindTexture(GL_TEXTURE_2D, beamtex->textureid);

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* child = (*it);

        //draw edge
        if(child->isVisible()) {
           splines[child].drawShadow();

           child->drawEdgeShadows(dt);
        }
    }
}

void RDirNode::drawEdges(float dt) {

    if(parent==0) glBindTexture(GL_TEXTURE_2D, beamtex->textureid);

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* child = (*it);

        //draw edge
        if(child->isVisible()) {
           splines[child].draw();

           child->drawEdges(dt);
        }
    }
}

void RDirNode::drawSimple(Frustum& frustum, float dt) {

    glDisable(GL_TEXTURE_2D);

    if(frustum.boundsInFrustum(quadItemBounds)) {
        glPushMatrix();
            glTranslatef(pos.x, pos.y, 0.0);
            for(std::list<RFile*>::iterator it = files.begin(); it!=files.end(); it++) {
                RFile* f = *it;
                f->drawSimple(dt);
            }
        glPopMatrix();
    }

    for(std::list<RDirNode*>::iterator it = children.begin(); it != children.end(); it++) {
        RDirNode* node = (*it);
        node->drawSimple(frustum,dt);
    }

    if(gGourceNodeDebug) {
        glColor4f(1.0, 1.0, 1.0, 1.0);

        vec2f vel_offset = pos + vel.normal() * 10.0;

        glBegin(GL_LINES);
            glVertex2fv(pos);
            glVertex2fv(vel_offset);
        glEnd();

        glColor4f(1.0, 1.0, 0.0, 1.0);

        glLineWidth(1.0);

        quadItemBounds.draw();
    }
}

void RDirNode::updateQuadItemBounds() {
    float radius = getRadius();

    //set bounds
    Bounds2D bounds;
    bounds.update(pos - vec2f(radius,radius));
    bounds.update(pos + vec2f(radius,radius));

    quadItemBounds = bounds;
}

